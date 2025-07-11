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

	static const size_t GRID_SIZE = 32;

	struct Light
	{
		glm::vec4 position_radius = glm::vec4(1);
		glm::vec4 direction_flux = glm::vec4(1);
		glm::vec4 color_type = glm::vec4(1);
		glm::vec4 position1 = glm::vec4(1);
		glm::vec4 position2 = glm::vec4(1);
		glm::vec4 uv0_uv1 = glm::vec4(1);
		glm::vec4 uv2_objectId_ = glm::vec4(1);
		glm::vec4 normal_area = glm::vec4(1);
	};

	struct SAliasTable
	{
		float threshold = 0;
		uint32_t indexA = 0;
		uint32_t indexB = 0;
		float weights = 0;
	};

	struct SCell
	{
		int32_t startIndex = -1;
		uint32_t numLights = 0;
		float weightsSum = 0;
		uint32_t pad1;
	};


	void init(VulkanEngine* engine);
	void load_config(std::string&& path);
	void save_config(std::string&& path);

	const AllocatedBuffer& get_light_buffer() const;
	const std::vector<VulkanLightManager::Light>& get_lights() const;
	const AllocatedBuffer& get_lights_alias_table_buffer() const;
	const AllocatedBuffer& get_lights_cell_grid_buffer() const;
	float get_WeightsSum();
	int32_t get_sun_index() const;
	const glm::vec3& get_grid_max() const;
	const glm::vec3& get_grid_min() const;

	bool is_sun_active() const;
	void add_sun_light(glm::vec3&& direction, glm::vec3&& color);
	void add_emission_light(glm::vec4& position, glm::vec4& position1, glm::vec4& position2, glm::vec2& uv0, glm::vec2& uv1, glm::vec2& uv2, uint32_t objectId);
	void update_sun_light(std::function<void(glm::vec3& direction, glm::vec3& color)>&& func);
	void generate_uniform_grid(glm::vec3 maxCube, glm::vec3 minCube, uint32_t lightNumber);

	void update_light_buffer();
	void create_light_buffer();
	void create_cpu_host_visible_light_buffer();


	void generate_lights_cell_grid();
	void update_lights_alias_table();

	void update_light_data_from_gpu();

private:

	void add_light_to_grid(uint32_t lightIndex, glm::vec3& gridMax, glm::vec3 gridMin);

	bool bUseCpuHostVisibleBuffer = false;

	int32_t sunIndex = -1;

	VulkanEngine* _engine = nullptr;

	std::vector<VulkanLightManager::Light> _lightsOnScene = {};
	AllocatedBuffer _lightsBuffer;

	std::vector<float> _lightsWeights = {};
	AllocatedBuffer  _lightsAliasTable;
	AllocatedBuffer _lightsCellGrid;

	std::vector<std::vector<std::vector<std::vector<uint32_t>>>> _grid;

	glm::vec3 _gridMax;
	glm::vec3 _gridMin;
};