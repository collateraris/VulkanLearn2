#include <vk_light_manager.h>

#include <vk_engine.h>
#include <time.h>
#include <vk_initializers.h>

glm::vec4 computeFaceNormalAndAreaW(glm::vec4& position, glm::vec4& position1, glm::vec4& position2)
{
	// Compute face normal in world space.
	// The length of the vector is twice the triangle area since we're in world space.
	// Note that this is not true if the normal is transformed using the inverse-transpose.
	glm::vec3 e[2];
	e[0] = glm::vec3(position1) - glm::vec3(position);
	e[1] = glm::vec3(position2) - glm::vec3(position);
	glm::vec3 N = cross(e[0], e[1]);
	float triangleArea = 0.5f * length(N);

	// Flip the normal depending on final winding order in world space.
	//if (isWorldFrontFaceCW(instanceID)) N = -N;

	return glm::vec4(glm::normalize(N), triangleArea);
}

glm::vec3 randomColor(std::uniform_int_distribution<>& dis, std::mt19937& gen)
{
	glm::vec3 color;
	for (int i = 0; i < 3; i++)
	{
		float val = dis(gen);

		color[i] = val / 255.f;
	}

	return color;
}

void VulkanLightManager::init(VulkanEngine* engine)
{
	_engine = engine;
}

void VulkanLightManager::load_config(std::string&& path)
{

}

void VulkanLightManager::save_config(std::string&& path)
{

}

void VulkanLightManager::create_light_buffer()
{
	assert(_lightsBuffer._buffer == VK_NULL_HANDLE);
	assert(!bUseCpuHostVisibleBuffer);

	uint32_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(_lightsOnScene.size() * sizeof(VulkanLightManager::Light));
	_lightsBuffer = _engine->create_buffer_n_copy_data(bufferSize, _lightsOnScene.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void VulkanLightManager::create_cpu_host_visible_light_buffer()
{
	bUseCpuHostVisibleBuffer = true;

	uint32_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(_lightsOnScene.size() * sizeof(VulkanLightManager::Light));
	_lightsBuffer = _engine->create_cpu_to_gpu_buffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	update_light_buffer();
}

void VulkanLightManager::generate_lights_cell_grid()
{
	uint32_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(GRID_SIZE * GRID_SIZE * GRID_SIZE * sizeof(VulkanLightManager::SCell));
	_lightsCellGrid = _engine->create_cpu_to_gpu_buffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}


const AllocatedBuffer& VulkanLightManager::get_lights_alias_table_buffer() const
{
	return _lightsAliasTable;
}

const AllocatedBuffer& VulkanLightManager::get_lights_cell_grid_buffer() const
{
	return _lightsCellGrid;
}

float VulkanLightManager::get_WeightsSum()
{
	float sum = 0;
	for (auto& w : _lightsWeights) { sum += w; }

	return sum;
}

int32_t VulkanLightManager::get_sun_index() const
{
	return sunIndex;
}

const glm::vec3& VulkanLightManager::get_grid_max() const
{
	return _gridMax;
}

const glm::vec3& VulkanLightManager::get_grid_min() const
{
	return _gridMin;
}

std::vector<VulkanLightManager::SAliasTable> create_alias_table(const std::vector<float>& weights, std::vector<uint32_t>& lightIndices)
{
	// Use >= since we reserve 0xFFFFFFFFu as an invalid flag marker during construction.
	if (weights.size() >= std::numeric_limits<uint32_t>::max()) throw std::exception("Too many entries for alias table.");

	std::uniform_int_distribution<uint32_t> rngDist;
	size_t weightsCount = weights.size();


	std::vector<float> weightsChangable = weights;


	std::vector<VulkanLightManager::SAliasTable> items(weights.size(), VulkanLightManager::SAliasTable{});

	// Our working set / intermediate buffers (underweight & overweight); initialize to "invalid"
	std::vector<uint32_t> lowIdx(weightsCount, 0xFFFFFFFFu);
	std::vector<uint32_t> highIdx(weightsCount, 0xFFFFFFFFu);

	// Sum element weights, use double to minimize precision issues
	float weightSum = 0.0;
	for (float f : weights) weightSum += f;

	// Find the average weight
	float avgWeight = float(weightSum / double(weightsCount));

	// Initialize working set. Inset inputs into our lists of above-average or below-average weight elements.
	int lowCount = 0;
	int highCount = 0;
	for (uint32_t i = 0; i < weightsCount; ++i)
	{
		if (weights[i] < avgWeight)
			lowIdx[lowCount++] = i;
		else
			highIdx[highCount++] = i;
	}

	// Create alias table entries by merging above- and below-average samples
	for (uint32_t i = 0; i < weightsCount; ++i)
	{
		// Usual case:  We have an above-average and below-average sample we can combine into one alias table entry
		if ((lowIdx[i] != 0xFFFFFFFFu) && (highIdx[i] != 0xFFFFFFFFu))
		{
			// Create an alias table tuple: 
			items[i] = { weightsChangable[lowIdx[i]] / avgWeight, lightIndices[highIdx[i]], lightIndices[lowIdx[i]], weights[i] };

			// We've removed some weight from element highIdx[i]; update it's weight, then re-enter it
			// on the end of either the above-average or below-average lists.
			float updatedWeight = (weightsChangable[lowIdx[i]] + weightsChangable[highIdx[i]]) - avgWeight;
			weightsChangable[highIdx[i]] = updatedWeight;
			if (updatedWeight < avgWeight)
				lowIdx[lowCount++] = highIdx[i];
			else
				highIdx[highCount++] = highIdx[i];
		}

		// The next two cases can only occur towards the end of table creation, because either:
		//    (a) all the remaining possible alias table entries have weight *exactly* equal to avgWeight,
		//        which means these alias table entries only have one input item that is selected
		//        with 100% probability
		//    (b) all the remaining alias table entires have *almost* avgWeight, but due to (compounding)
		//        precision issues throughout the process, they don't have *quite* that value.  In this case
		//        treating these entries as having exactly avgWeight (as in case (a)) is the only right
		//        thing to do mathematically (other than re-generating the alias table using higher precision
		//        or trying to reduce catasrophic numerical cancellation in the "updatedWeight" computation above).
		else if (highIdx[i] != 0xFFFFFFFFu)
		{
			items[i] = { 1.0f, lightIndices[highIdx[i]], lightIndices[highIdx[i]], weights[i] };
		}
		else if (lowIdx[i] != 0xFFFFFFFFu)
		{
			items[i] = { 1.0f, lightIndices[lowIdx[i]], lightIndices[lowIdx[i]], weights[i] };
		}

		// If there is neither a highIdx[i] or lowIdx[i] for some array element(s).  By construction, 
		// this cannot occur (without some logic bug above).
		else
		{
			assert(false); // Should not occur
		}
	}

	return items;
}


void VulkanLightManager::update_lights_alias_table()
{
	_gridMax = { std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min() };
	_gridMin = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	for (int lightIndex = 0; lightIndex < _lightsOnScene.size() && lightIndex < VULKAN_MAX_LIGHT_COUNT; lightIndex++)
	{
		glm::vec3 lightPos = glm::vec3(1);
		Light& light = _lightsOnScene[lightIndex];
		ELightType lightType = static_cast<ELightType>(light.color_type.w);

		if (lightType == ELightType::Sun)
		{
			continue;
		}
		else if (lightType == ELightType::Point)
		{
			lightPos = light.position_radius;
		}
		else if (lightType == ELightType::Emission)
		{
			lightPos = (light.position_radius + light.position1 + light.position2) / 3;
		}

		glm::vec3 minBound = lightPos - 2 * light.position_radius.w;
		glm::vec3 maxBound = lightPos + 2 * light.position_radius.w;

		_gridMax.x = std::max(_gridMax.x, maxBound.x);
		_gridMax.y = std::max(_gridMax.y, maxBound.y);
		_gridMax.z = std::max(_gridMax.z, maxBound.z);

		_gridMin.x = std::min(_gridMin.x, minBound.x);
		_gridMin.y = std::min(_gridMin.y, minBound.y);
		_gridMin.z = std::min(_gridMin.z, minBound.z);
	}

	_grid.resize(GRID_SIZE);
	for (size_t i = 0; i < GRID_SIZE; i++)
	{
		_grid[i].resize(GRID_SIZE);
		for (size_t j = 0; j < GRID_SIZE; j++)
		{
			_grid[i][j].resize(GRID_SIZE);
		}
	}


	for (int i = 0; i < _lightsOnScene.size() && i < VULKAN_MAX_LIGHT_COUNT; i++)
	{
		add_light_to_grid(i, _gridMax, _gridMin);
	}

	std::vector<SAliasTable> bigAliasTable = {};

	std::vector<SCell> cellTable(GRID_SIZE * GRID_SIZE * GRID_SIZE);

	for (size_t i = 0; i < GRID_SIZE; i++)
	{
		for (size_t j = 0; j < GRID_SIZE; j++)
		{
			for (size_t k = 0; k < GRID_SIZE; k++)
			{
				std::vector<uint32_t>& lightsIndices = _grid[i][j][k];

				const uint32_t linearIdx = i * GRID_SIZE * GRID_SIZE + j * GRID_SIZE + k;

				SCell& cell = cellTable[linearIdx];

				cell.startIndex = -1;

				if (lightsIndices.size() == 0)
					continue;

				std::vector<float> lightsWeights = {};
				float weightsSum = 0;
				for (size_t x = 0; x < lightsIndices.size(); x++)
				{
					float w = _lightsWeights[lightsIndices[x]];
					weightsSum += w;
					lightsWeights.push_back(w);
				}

				std::vector<SAliasTable> table = create_alias_table(lightsWeights, lightsIndices);

				uint32_t startIndex = bigAliasTable.size();

				for (auto& t: table)
				{
					bigAliasTable.push_back(t);
				}

				cell.weightsSum = weightsSum;
				cell.startIndex = startIndex;
				cell.numLights = lightsIndices.size();
			}
		}
	}

	_grid.clear();


	uint32_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(bigAliasTable.size() * sizeof(VulkanLightManager::SAliasTable));
	_lightsAliasTable = _engine->create_cpu_to_gpu_buffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	_engine->map_buffer(_engine->_allocator, _lightsAliasTable._allocation, [&](void*& data) {
		SAliasTable* tableItem = (SAliasTable*)data;
		for (int i = 0; i < bigAliasTable.size() && i < VULKAN_MAX_LIGHT_COUNT; i++)
		{
			const SAliasTable& object = bigAliasTable[i];
			tableItem[i].threshold = object.threshold;
			tableItem[i].weights = object.weights;
			tableItem[i].indexA = object.indexA;
			tableItem[i].indexB = object.indexB;
		}
	});

	bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(cellTable.size() * sizeof(VulkanLightManager::SCell));
	_lightsCellGrid = _engine->create_cpu_to_gpu_buffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	_engine->map_buffer(_engine->_allocator, _lightsCellGrid._allocation, [&](void*& data) {
		SCell* tableItem = (SCell*)data;
		for (int i = 0; i < cellTable.size() && i < VULKAN_MAX_LIGHT_COUNT; i++)
		{
			const SCell& object = cellTable[i];
			tableItem[i].startIndex = object.startIndex;
			tableItem[i].numLights = object.numLights;
			tableItem[i].weightsSum = object.weightsSum;
		}
	});
}

void VulkanLightManager::update_light_data_from_gpu()
{
	_lightsWeights.clear();

	uint32_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(_lightsOnScene.size() * sizeof(VulkanLightManager::Light));
	AllocatedBuffer staggingBuffer = _engine->create_staging_buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {
			std::array<VkBufferMemoryBarrier, 1> barriers =
			{
				vkinit::buffer_barrier(get_light_buffer()._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT),
			};

			vkCmdPipelineBarrier(cmd.get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);

			cmd.copy_buffer(staggingBuffer, 0, _lightsBuffer, 0, bufferSize);

			std::array<VkBufferMemoryBarrier, 1> barriers2 =
			{
				vkinit::buffer_barrier(get_light_buffer()._buffer, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT),
			};

			vkCmdPipelineBarrier(cmd.get_cmd(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, barriers2.size(), barriers2.data(), 0, 0);
		});

	_lightsWeights.resize(_lightsOnScene.size());

	_engine->map_buffer(_engine->_allocator, staggingBuffer._allocation, [&](void*& data) {
		VulkanLightManager::Light* lightSSBO = (VulkanLightManager::Light*)data;
		for (int i = 0; i < _lightsOnScene.size() && i < VULKAN_MAX_LIGHT_COUNT; i++)
		{
			_lightsOnScene[i] = lightSSBO[i];
			_lightsWeights[i] = _lightsOnScene[i].direction_flux[3];
		}
	});

	_engine->destroy_buffer(_engine->_allocator, staggingBuffer);
}

void VulkanLightManager::add_light_to_grid(uint32_t lightIndex,glm::vec3& gridMax, glm::vec3 gridMin)
{
	Light& light = _lightsOnScene[lightIndex];
	ELightType lightType = static_cast<ELightType>(light.color_type.w);
	
	glm::vec3 lightPos = glm::vec3(1);

	if (lightType == ELightType::Sun)
	{
		for (size_t i = 0; i < GRID_SIZE; i++)
			for (size_t j = 0; j < GRID_SIZE; j++)
				for (size_t k = 0; k < GRID_SIZE; k++)
					_grid[i][j][k].push_back(lightIndex);
		return;
	}
	else if (lightType == ELightType::Point)
	{
		lightPos = light.position_radius;
	}
	else if (lightType == ELightType::Emission)
	{
		lightPos = (light.position_radius + light.position1 + light.position2) / 3;
	}

	float radius = light.position_radius.w;
	glm::vec3 minBound = lightPos - radius;
	glm::vec3 maxBound = lightPos + radius;

	glm::vec3 size = gridMax - gridMin;

	glm::vec3 gridMinBound = (minBound - gridMin) / size;
	glm::vec3 gridMaxBound = (maxBound - gridMin) / size;


	glm::uvec3 startCell = clamp(glm::uvec3(gridMinBound * GRID_SIZE), glm::uvec3(0), glm::uvec3(GRID_SIZE) - glm::uvec3(1));
	glm::uvec3 endCell = clamp(glm::uvec3(gridMaxBound * GRID_SIZE), glm::uvec3(0), glm::uvec3(GRID_SIZE) - glm::uvec3(1));

	for (size_t z = startCell.z; z <= endCell.z; ++z) {
		for (size_t y = startCell.y; y <= endCell.y; ++y) {
			for (size_t x = startCell.x; x <= endCell.x; ++x) {
				_grid[x][y][z].push_back(lightIndex);
			}
		}
	}
}

const AllocatedBuffer& VulkanLightManager::get_light_buffer() const
{
	return _lightsBuffer;
}

const std::vector<VulkanLightManager::Light>& VulkanLightManager::get_lights() const
{
	return _lightsOnScene;
}

bool VulkanLightManager::is_sun_active() const
{
	return sunIndex >= 0;
}

void VulkanLightManager::add_sun_light(glm::vec3&& direction, glm::vec3&& color)
{
	sunIndex = _lightsOnScene.size();
	_lightsOnScene.push_back({
					.direction_flux = glm::vec4(direction, 1.f),
					.color_type = glm::vec4(color, static_cast<uint32_t>(ELightType::Sun))
		});
}

void VulkanLightManager::add_emission_light(glm::vec4& position, glm::vec4& position1, glm::vec4& position2, glm::vec2& uv0, glm::vec2& uv1, glm::vec2& uv2, uint32_t objectId)
{
	//if (_lightsOnScene.size() > 2000) return;
	_lightsOnScene.push_back({
					.position_radius = position,
					.direction_flux = glm::vec4(1),
					.color_type = glm::vec4(glm::vec3(1., 1., 1.), static_cast<uint32_t>(ELightType::Emission)),
					.position1 = position1,
					.position2 = position2,
					.uv0_uv1 = glm::vec4(uv0.x, uv0.y, uv1.x, uv1.y),
					.uv2_objectId_ = glm::vec4(uv2.x, uv2.y, objectId, 1.f),
					.normal_area = computeFaceNormalAndAreaW(position, position1, position2)
		});
}


void VulkanLightManager::update_sun_light(std::function<void(glm::vec3& direction, glm::vec3& color)>&& func)
{
	auto& sunInfo = _lightsOnScene[sunIndex];
	glm::vec3 direction = sunInfo.direction_flux;
	direction = normalize(direction);
	glm::vec3 color = sunInfo.color_type;

	func(direction, color);

	sunInfo.direction_flux = glm::vec4(direction, sunInfo.direction_flux.w);
	sunInfo.color_type = glm::vec4(color, static_cast<uint32_t>(ELightType::Sun));

}

void VulkanLightManager::generate_uniform_grid(glm::vec3 maxCube, glm::vec3 minCube, uint32_t lightNumber)
{
	float stepX = std::abs(maxCube.x - minCube.x) / static_cast<float>(lightNumber);
	float stepY = std::abs(maxCube.y - minCube.y) / static_cast<float>(lightNumber);
	float stepZ = std::abs(maxCube.z - minCube.z) / static_cast<float>(lightNumber);

	// Random seed
	std::random_device rd;

	// Initialize Mersenne Twister pseudo-random number generator
	std::mt19937 gen(rd());

	// Generate pseudo-random numbers
	// uniformly distributed in range (1, 100)
	std::uniform_int_distribution<> dis(0, 255);

	for (float posx = minCube.x; posx < maxCube.x; posx+= stepX)
	{
		for (float posy = minCube.y; posy < maxCube.y; posy += stepY)
		{
			for (float posz = minCube.z; posz < maxCube.z; posz += stepZ)
			{
				_lightsOnScene.push_back({
					.position_radius = glm::vec4(posx, posy, posz, 1.f),
					.color_type = glm::vec4(randomColor(dis, gen), static_cast<uint32_t>(ELightType::Point))
				});
			}
		}
	}
}

void VulkanLightManager::update_light_buffer()
{
	_engine->map_buffer(_engine->_allocator, _lightsBuffer._allocation, [&](void*& data) {
		VulkanLightManager::Light* lightSSBO = (VulkanLightManager::Light*)data;
		for (int i = 0; i < _lightsOnScene.size() && i < VULKAN_MAX_LIGHT_COUNT; i++)
		{
			const VulkanLightManager::Light& object = _lightsOnScene[i];
			lightSSBO[i].direction_flux = object.direction_flux;
			lightSSBO[i].position_radius = object.position_radius;
			lightSSBO[i].color_type = object.color_type;
			lightSSBO[i].position1 = object.position1;
			lightSSBO[i].position2 = object.position2;
			lightSSBO[i].uv0_uv1 = object.uv0_uv1;
			lightSSBO[i].uv2_objectId_ = object.uv2_objectId_;
			lightSSBO[i].normal_area = object.normal_area;
		}
		});
}
