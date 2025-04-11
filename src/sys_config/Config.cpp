#include  <sys_config/Config.h>
#include <sys_config/vk_strings.h>

#include <cassert>

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

ERenderMode vk_utils::Config::GetRenderMode()
{
    static ERenderMode mode = ERenderMode::None;
    if (mode == ERenderMode::None)
    {
        XPath config = GetRoot().GetPath("render_mode");
        std::string name = config.GetAttribute<std::string>("name");

        if (!name.compare("PATHTRACER"))
            mode = ERenderMode::Pathtracer;
        else if (!name.compare("RESTIR_GI"))
            mode = ERenderMode::ReSTIR_GI;

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

