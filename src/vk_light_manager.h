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

	bool is_sun_active() const;
	void add_sun_light(glm::vec3&& direction, glm::vec3&& color);
	void update_sun_light(std::function<void(glm::vec3& direction, glm::vec3& color)>&& func);
	void generate_uniform_grid(glm::vec3 maxCube, glm::vec3 minCube, uint32_t lightNumber);

	void update_light_buffer();
	void create_light_buffer();
	void create_cpu_host_visible_light_buffer();

	void set_emissive_count(size_t size) { _emissiveCount = size;}
	size_t get_emissive_count() { return _emissiveCount;};

private:

	bool bUseCpuHostVisibleBuffer = false;

	int32_t sunIndex = -1;

	size_t _emissiveCount = 0;

	VulkanEngine* _engine = nullptr;

	std::vector<VulkanLightManager::Light> _lightsOnScene = {};
	AllocatedBuffer _lightsBuffer;
};