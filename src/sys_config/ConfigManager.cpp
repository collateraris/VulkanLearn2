#include  <sys_config/ConfigManager.h>

using namespace vk_utils;

ConfigManager::ConfigManager()
{
}

ConfigManager::~ConfigManager()
{
}

ConfigManager& ConfigManager::Get()
{
	static ConfigManager configManager;
	return configManager;
}

XPath ConfigManager::GetRoot(const std::string configPath)
{
	auto it = m_configsMap.find(configPath);
	if (it == m_configsMap.end())
		m_configsMap[configPath] = std::make_unique<Config>(configPath);

	return m_configsMap[configPath]->GetRoot();
}

Config& vk_utils::ConfigManager::GetConfig(const std::string configPath)
{
	auto it = m_configsMap.find(configPath);
	if (it == m_configsMap.end())
		m_configsMap[configPath] = std::make_unique<Config>(configPath);

	return *m_configsMap[configPath];
}
