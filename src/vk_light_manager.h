#pragma once 

#include <vk_types.h>

enum class ELightType : uint32_t
{
	None = 0,
	Sun = 1,
};


class VulkanEngine;

class VulkanLightManager
{
public:
	struct Light
	{
		glm::vec4 position;
		glm::vec4 direction;
		uint32_t type = static_cast<uint32_t>(ELightType::None);
		uint32_t pad1;
		uint32_t pad2;
		uint32_t pad3;
	};

	void init(VulkanEngine* engine);
	void load_config(std::string&& path);
	void save_config(std::string&& path);

	VulkanLightManager::Light* add_light(VulkanLightManager::Light&& lightInfo);
	void create_light_buffer();
	void update_light_buffer();
	const AllocatedBuffer& get_light_buffer() const;

private:

	VulkanEngine* _engine = nullptr;

	std::vector<std::unique_ptr<VulkanLightManager::Light>> _lightsOnScene = {};
	AllocatedBuffer _lightsBuffer;
};