#pragma once 

#include <vk_types.h>

enum class ELightType : uint32_t
{
	None = 0,
	Sun = 1,
	Point = 2,
};


class VulkanEngine;

class VulkanLightManager
{
public:
	struct Light
	{
		glm::vec4 position;
		glm::vec4 direction;
		glm::vec4 color;
		uint32_t type = static_cast<uint32_t>(ELightType::None);
		uint32_t pad1;
		uint32_t pad2;
		uint32_t pad3;
	};

	void init(VulkanEngine* engine);
	void load_config(std::string&& path);
	void save_config(std::string&& path);

	VulkanLightManager::Light* add_light(VulkanLightManager::Light&& lightInfo);
	void add_sun_light();
	VulkanLightManager::Light* get_sun_light();
	void create_light_buffers();
	void create_light_buffer(int current_frame_index);
	void update_light_buffer(int current_frame_index);
	const AllocatedBuffer& get_light_buffer(int current_frame_index) const;
	const std::vector<std::unique_ptr<VulkanLightManager::Light>>& get_lights() const;

	void generateUniformGrid(glm::vec3 maxCube, glm::vec3 minCube, uint32_t lightNumber);

private:

	VulkanEngine* _engine = nullptr;

	std::vector<std::unique_ptr<VulkanLightManager::Light>> _lightsOnScene = {};
	std::array<AllocatedBuffer, 2> _lightsBuffer;
};