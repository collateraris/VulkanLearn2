#include <sys_config/Config.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    std::filesystem::path directory;
    std::filesystem::path file;

    Fixture()
    {
        std::random_device random;
        for (int attempt = 0; attempt < 32; ++attempt) {
            directory = std::filesystem::temp_directory_path() /
                ("restir-config-save-" + std::to_string(random()) + "-" + std::to_string(random()));
            if (std::filesystem::create_directory(directory)) {
                file = directory / "config.xml";
                return;
            }
        }
        throw std::runtime_error("Could not create an isolated configuration fixture");
    }

    ~Fixture()
    {
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        std::filesystem::remove(directory, ignored);
    }

    void write(const std::string& text) const
    {
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output << text;
        output.close();
        check(bool(output), "Could not write test configuration");
    }

    std::string read() const
    {
        std::ifstream input(file, std::ios::binary);
        check(bool(input), "Could not read test configuration");
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
};

const char* initial = R"xml(<?xml version="1.0"?>
<config>
  <window title="Original" width="1200" height="800"/>
  <current_scene id="0"/>
  <render_mode name="RESTIR"/>
  <upscaling mode="off"/>
  <scene_configs><scene id="0" environmentIntensity="0" indirectSunScale="0.015625" camPos_axisX="0"/></scene_configs>
</config>)xml";

const char* externallyEdited = R"xml(<?xml version="1.0"?>
<config projectNote="preserve me">
  <!-- Preserve scene edits made while the renderer is open. -->
  <window title="Changed outside the renderer" width="1980" height="1080" customWindow="keep"/>
  <current_scene id="1"/>
  <render_mode name="RESTIR_NRC"/>
  <upscaling mode="performance" customUpscaling="keep"/>
  <scene_configs>
    <scene id="0" environmentIntensity="0.75" indirectSunScale="0.5" camPos_axisX="17"/>
    <scene id="1" path="sponza.obj" hdr="new-sky.hdr" environmentIntensity="1" indirectSunScale="1" useSun="1" sunColor_axisX="3" camPos_axisX="-950" camPitch="0.3"/>
  </scene_configs>
  <futureSetting enabled="1"/>
</config>)xml";

void latest_scene_edits_survive_apply()
{
    Fixture fixture;
    fixture.write(initial);
    vk_utils::Config config(fixture.file.string());
    fixture.write(externallyEdited);
    check(config.SaveRenderSettings({2560, 1440, 1}), "Apply failed for valid external edits");

    tinyxml2::XMLDocument saved;
    check(saved.LoadFile(fixture.file.string().c_str()) == tinyxml2::XML_SUCCESS, "Saved XML is invalid");
    auto* root = saved.FirstChildElement("config");
    auto* window = root->FirstChildElement("window");
    auto* upscaling = root->FirstChildElement("upscaling");
    check(window->IntAttribute("width") == 2560 && window->IntAttribute("height") == 1440,
        "Requested output dimensions were not saved");
    check(std::string(upscaling->Attribute("mode")) == "quality", "Requested DLSS mode was not saved");

    // Remove the three intentionally changed values before comparing the full
    // DOM: every scene, unknown attribute, comment and unrelated node must match.
    window->SetAttribute("width", 1980);
    window->SetAttribute("height", 1080);
    upscaling->SetAttribute("mode", "performance");
    tinyxml2::XMLDocument expected;
    check(expected.Parse(externallyEdited) == tinyxml2::XML_SUCCESS, "Invalid test fixture");
    tinyxml2::XMLPrinter actualText, expectedText;
    saved.Print(&actualText);
    expected.Print(&expectedText);
    check(std::string(actualText.CStr()) == expectedText.CStr(), "Apply changed externally edited scene data");

    const auto settings = config.GetRenderSettings();
    check(settings.outputWidth == 2560 && settings.outputHeight == 1440 && settings.dlssMode == 1,
        "In-memory display settings did not refresh after saving");
    check(std::string(config.GetRoot().GetPath("window").GetElement()->Attribute("title")) ==
        "Changed outside the renderer", "In-memory scene document stayed stale");
    check(config.GetRoot().GetPath("current_scene").GetAttribute<int>("id") == 1,
        "In-memory current scene stayed stale");

    // The next Apply must reload again, rather than relying on the first refresh.
    std::string editedAgain = externallyEdited;
    const auto position = editedAgain.find("environmentIntensity=\"1\"");
    editedAgain.replace(position, std::string("environmentIntensity=\"1\"").size(), "environmentIntensity=\"2\"");
    fixture.write(editedAgain);
    check(config.SaveRenderSettings({1920, 1080, 3}), "Second Apply failed");
    check(fixture.read().find("environmentIntensity=\"2\"") != std::string::npos,
        "Second Apply lost a later lighting edit");
}

void absent_upscaling_is_created()
{
    Fixture fixture;
    fixture.write(initial);
    vk_utils::Config config(fixture.file.string());
    fixture.write("<config><window width=\"640\" height=\"480\" title=\"keep\"/>"
        "<scene_configs><scene environmentIntensity=\"1\" indirectSunScale=\"1\"/></scene_configs></config>");
    check(config.SaveRenderSettings({1280, 720, 2}), "Apply could not add an absent upscaling node");
    check(config.GetRenderSettings().dlssMode == 2, "New upscaling setting is missing");
    check(fixture.read().find("environmentIntensity=\"1\"") != std::string::npos,
        "Adding upscaling lost lighting attributes");
}

void invalid_input_is_not_overwritten()
{
    Fixture fixture;
    fixture.write(initial);
    vk_utils::Config config(fixture.file.string());
    for (const std::string text : {
        "<config><window></config>",
        "",
        "<different><window width=\"640\" height=\"480\"/></different>",
        "<config><scene_configs/></config>",
        "<config><window/></config><secondRoot/>"}) {
        fixture.write(text);
        check(!config.SaveRenderSettings({2560, 1440, 1}), "Invalid XML/configuration was accepted");
        check(fixture.read() == text, "Rejected configuration was overwritten");
        const auto settings = config.GetRenderSettings();
        check(settings.outputWidth == 1200 && settings.outputHeight == 800 && settings.dlssMode == 0,
            "Failed save changed the in-memory document");
    }
    check(std::filesystem::remove(fixture.file), "Could not remove missing-file fixture");
    check(!config.SaveRenderSettings({2560, 1440, 1}), "Apply accepted a missing configuration");
    check(!std::filesystem::exists(fixture.file), "Apply recreated a missing configuration from stale data");

    fixture.write(externallyEdited);
    for (const RenderSettings settings : {RenderSettings{0, 1440, 1}, RenderSettings{2560, 0, 1},
        RenderSettings{7681, 1440, 1}, RenderSettings{2560, 4321, 1},
        RenderSettings{2560, 1440, -1}, RenderSettings{2560, 1440, 6}}) {
        check(!config.SaveRenderSettings(settings), "Out-of-range display settings were accepted");
        check(fixture.read() == externallyEdited, "Invalid display settings changed the file");
    }
}
}

int main()
{
    try {
        latest_scene_edits_survive_apply();
        absent_upscaling_is_created();
        invalid_input_is_not_overwritten();
        std::cout << "Config save regression tests passed (3 groups, 15 save attempts).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Config save regression failed: " << error.what() << '\n';
        return 1;
    }
}
