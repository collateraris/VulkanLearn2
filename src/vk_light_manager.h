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
	struct alignas(16) Light
	{
		glm::vec4 position;
		glm::vec4 direction;
		glm::vec4 color_type;
	};

	void init(VulkanEngine* engine);
	void load_config(std::string&& path);
	void save_config(std::string&& path);

	const AllocatedBuffer& get_light_buffer() const;
	const std::vector<VulkanLightManager::Light>& get_lights() const;

	void generateUniformGrid(glm::vec3 maxCube, glm::vec3 minCube, uint32_t lightNumber);

private:
	void create_light_buffer();

	VulkanEngine* _engine = nullptr;

	std::vector<VulkanLightManager::Light> _lightsOnScene = {};
	AllocatedBuffer _lightsBuffer;
};