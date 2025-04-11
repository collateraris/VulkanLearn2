#pragma once

#include <vk_types.h>
#include  <sys_config/XPath.h>

namespace vk_utils
{
    class Config
    {
    public:
        Config(const std::string fileName) : mFileName(fileName) { Load(); }
        ~Config() {  }

        XPath GetRoot() const { return XPath(mDocument.FirstChildElement()); };

        std::string GetTitle();
        uint32_t GetWindowWidth();
        uint32_t GetWindowHeight();
        ERenderMode GetRenderMode();
        SceneConfig GetCurrentScene();
        uint32_t GetEnvMapSize();
        uint32_t GetIrradianceSize();

    private:
        std::string mFileName;
        tinyxml2::XMLDocument mDocument;

        void Load();
        void Save();
    };
}

