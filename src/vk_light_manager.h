#pragma once 

#include <vk_types.h>

enum class ELightType : uint32_t
{
	None = 0,
	Sun = 1,
	Point = 2,
	Emission = 3,
};


class VulkanEngine;

class VulkanLightManager
{
public:
	struct Light
	{
		glm::vec4 position = glm::vec4(1);
		glm::vec4 direction = glm::vec4(1);
		glm::vec4 color_type = glm::vec4(1);
		glm::vec4 position1 = glm::vec4(1);
		glm::vec4 position2 = glm::vec4(1);
	};

	void init(VulkanEngine* engine);
	void load_config(std::string&& path);
	void save_config(std::string&& path);

	const AllocatedBuffer& get_light_buffer() const;
	const std::vector<VulkanLightManager::Light>& get_lights() const;

	bool is_sun_active() const;
	void add_sun_light(glm::vec3&& direction, glm::vec3&& color);
	void add_emission_light(glm::vec4& position, glm::vec4& position1, glm::vec4& position2);
	void update_sun_light(std::function<void(glm::vec3& direction, glm::vec3& color)>&& func);
	void generate_uniform_grid(glm::vec3 maxCube, glm::vec3 minCube, uint32_t lightNumber);

	void update_light_buffer();
	void create_light_buffer();
	void create_cpu_host_visible_light_buffer();

private:

	bool bUseCpuHostVisibleBuffer = false;

	int32_t sunIndex = -1;

	VulkanEngine* _engine = nullptr;

	std::vector<VulkanLightManager::Light> _lightsOnScene = {};
	AllocatedBuffer _lightsBuffer;
};