#include  <sys_config/Config.h>
#include <sys_config/vk_strings.h>

#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace vk_utils;

std::string vk_utils::Config::GetTitle()
{
    XPath windowConfig = GetRoot().GetPath("window");

    return windowConfig.GetAttribute<std::string>("title");
}

uint32_t vk_utils::Config::GetWindowWidth()
{
    XPath windowConfig = GetRoot().GetPath("window");

    return windowConfig.GetAttribute<uint32_t>("width");
}

uint32_t vk_utils::Config::GetWindowHeight()
{
    XPath windowConfig = GetRoot().GetPath("window");

    return  windowConfig.GetAttribute<uint32_t>("height");
}

RenderSettings vk_utils::Config::GetRenderSettings()
{
    RenderSettings settings{GetWindowWidth(), GetWindowHeight(), 0};
    const auto* upscaling = mDocument.FirstChildElement()->FirstChildElement("upscaling");
    const char* mode = upscaling ? upscaling->Attribute("mode") : nullptr;
    static const char* modes[] = {"off", "quality", "balanced", "performance", "ultra_performance", "dlaa"};
    if (mode)
    {
        bool recognized = false;
        for (int i = 0; i < 6; ++i)
            if (std::string(mode) == modes[i]) { settings.dlssMode = i; recognized = true; break; }
        if (!recognized) throw std::runtime_error("Invalid upscaling mode in config.xml");
    }
    return settings;
}

bool vk_utils::Config::SaveRenderSettings(const RenderSettings& settings)
{
    if (settings.outputWidth < 320 || settings.outputHeight < 200 ||
        settings.outputWidth > 7680 || settings.outputHeight > 4320 ||
        settings.dlssMode < 0 || settings.dlssMode > 5) return false;
    auto* root = mDocument.FirstChildElement();
    auto* window = root->FirstChildElement("window");
    window->SetAttribute("width", settings.outputWidth);
    window->SetAttribute("height", settings.outputHeight);
    auto* upscaling = root->FirstChildElement("upscaling");
    if (!upscaling) { upscaling = mDocument.NewElement("upscaling"); root->InsertEndChild(upscaling); }
    static const char* modes[] = {"off", "quality", "balanced", "performance", "ultra_performance", "dlaa"};
    upscaling->SetAttribute("mode", modes[settings.dlssMode]);
    return mDocument.SaveFile(mFileName.c_str()) == tinyxml2::XML_SUCCESS;
}

ERenderMode vk_utils::Config::GetRenderMode()
{
    static ERenderMode mode = ERenderMode::None;
    if (mode == ERenderMode::None)
    {
        XPath config = GetRoot().GetPath("render_mode");
        std::string name = config.GetAttribute<std::string>("name");

        if (!name.compare("PATHTRACER"))
            mode = ERenderMode::Pathtracer;
        else if (!name.compare("RESTIR"))
            mode = ERenderMode::ReSTIR;
        else if (!name.compare("RESTIR_NRC"))
            mode = ERenderMode::ReSTIR_NRC;    

    }
    return mode;
}

SceneConfig vk_utils::Config::GetCurrentScene()
{
    SceneConfig config;
    XPath currentSceneConfig = GetRoot().GetPath("current_scene");
    uint32_t id = currentSceneConfig.GetAttribute<uint32_t>("id");
    auto sceneConfigs = GetRoot().GetPath("scene_configs").GetChildren();
    auto sceneConfig = sceneConfigs[id];

    config.fileName = vk_utils::ASSETS_PATH + sceneConfig.GetAttribute<std::string>("path");
    config.hdrCubemapPath = vk_utils::ASSETS_PATH + sceneConfig.GetAttribute<std::string>("hdr");
    const auto nonnegativeAttribute = [&](const char* name, float fallback) {
        float value = fallback;
        const auto* element = sceneConfig.GetElement();
        if ((element->Attribute(name) && element->QueryFloatAttribute(name, &value) != tinyxml2::XML_SUCCESS)
            || !std::isfinite(value) || value < 0.f)
            throw std::runtime_error(std::string("Invalid non-negative scene attribute: ") + name);
        return value;
    };
    config.environmentIntensity = nonnegativeAttribute("environmentIntensity", config.environmentIntensity);
    config.indirectSunScale = nonnegativeAttribute("indirectSunScale", config.indirectSunScale);
    config.scaleFactor = sceneConfig.GetAttribute<float>("scaleFactor");
    bool bNeedRotation = sceneConfig.GetAttribute<int>("needRotation");

    if (bNeedRotation)
    {
        float radians = sceneConfig.GetAttribute<float>("radians");
        float axisX = sceneConfig.GetAttribute<int>("axisX");
        float axisY = sceneConfig.GetAttribute<int>("axisY");
        float axisZ = sceneConfig.GetAttribute<int>("axisZ");

        config.model = glm::rotate(config.model, glm::radians(radians), glm::vec3(axisX, axisY, axisZ));
    }

    config.lightConfig.bUseSun = sceneConfig.GetAttribute<int>("useSun");
    if (config.lightConfig.bUseSun)
    {
        config.lightConfig.sunDirection[0] = sceneConfig.GetAttribute<float>("sunDirection_axisX");
        config.lightConfig.sunDirection[1] = sceneConfig.GetAttribute<float>("sunDirection_axisY");
        config.lightConfig.sunDirection[2] = sceneConfig.GetAttribute<float>("sunDirection_axisZ");

        config.lightConfig.sunColor[0] = sceneConfig.GetAttribute<float>("sunColor_axisX");
        config.lightConfig.sunColor[1] = sceneConfig.GetAttribute<float>("sunColor_axisY");
        config.lightConfig.sunColor[2] = sceneConfig.GetAttribute<float>("sunColor_axisZ");
    }

    config.lightConfig.bUseUniformGeneratePointLight = sceneConfig.GetAttribute<int>("useUniformGeneratePointLight");
    if (config.lightConfig.bUseUniformGeneratePointLight)
    {
        config.lightConfig.numUniformPointLightPerAxis = sceneConfig.GetAttribute<int>("numUniformPointLightPerAxis");
    }

    config.bUseCustomCam = sceneConfig.GetAttribute<int>("useCustomCameraPos");
    if (config.bUseCustomCam)
    {
        config.camPos[0] = sceneConfig.GetAttribute<float>("camPos_axisX");
        config.camPos[1] = sceneConfig.GetAttribute<float>("camPos_axisY");
        config.camPos[2] = sceneConfig.GetAttribute<float>("camPos_axisZ");
        config.camPith = sceneConfig.GetAttribute<float>("camPitch");
        config.camYaw = sceneConfig.GetAttribute<float>("camYaw");
    }


    return config;
}

uint32_t vk_utils::Config::GetEnvMapSize()
{
    XPath envConfig = GetRoot().GetPath("env_map");

    return  envConfig.GetAttribute<uint32_t>("size");
}

uint32_t vk_utils::Config::GetIrradianceSize()
{
    XPath envConfig = GetRoot().GetPath("irradiance_map");

    return  envConfig.GetAttribute<uint32_t>("size");
}

void Config::Load()
{
    bool flag = (mDocument.LoadFile(mFileName.c_str()) == tinyxml2::XML_SUCCESS);
    assert(flag);
    (void)flag;
}

void Config::Save()
{
    bool flag = (mDocument.SaveFile(mFileName.c_str()) == tinyxml2::XML_SUCCESS);
    assert(flag);
    (void)flag;
}

