#pragma once

#include  <sys_config/Config.h>

#include <memory>
#include <unordered_map>

namespace vk_utils
{

    class ConfigManager
    {
    public:

        static ConfigManager& Get();

        XPath GetRoot(const std::string configPath);

        Config& GetConfig(const std::string configPath);

    private:

        ConfigManager();
        ~ConfigManager();
        ConfigManager(ConfigManager&) = delete;
        ConfigManager(ConfigManager&&) = delete;
        void operator=(ConfigManager&) = delete;
        void operator=(ConfigManager&&) = delete;


        std::unordered_map < std::string, std::unique_ptr<Config>> m_configsMap;
    };
}
