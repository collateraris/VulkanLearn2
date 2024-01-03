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

SceneConfig vk_utils::Config::GetCurrentScene()
{
    SceneConfig config;
    XPath currentSceneConfig = GetRoot().GetPath("current_scene");
    uint32_t id = currentSceneConfig.GetAttribute<uint32_t>("id");
    auto sceneConfigs = GetRoot().GetPath("scene_configs").GetChildren();
    auto sceneConfig = sceneConfigs[id];

    config.fileName = vk_utils::ASSETS_PATH + sceneConfig.GetAttribute<std::string>("path");
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

    return config;
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

