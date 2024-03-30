#include <vk_light_manager.h>

#include <vk_engine.h>
#include <time.h>

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

	_lightsBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanLightManager::Light) * _lightsOnScene.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	update_light_buffer();
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
					.direction = glm::vec4(direction, 1.f),
					.color_type = glm::vec4(color, static_cast<uint32_t>(ELightType::Sun))
		});
}

void VulkanLightManager::update_sun_light(std::function<void(glm::vec3& direction, glm::vec3& color)>&& func)
{
	auto& sunInfo = _lightsOnScene[sunIndex];
	glm::vec3 direction = sunInfo.direction;
	direction = normalize(direction);
	glm::vec3 color = sunInfo.color_type;

	func(direction, color);

	sunInfo.direction = glm::vec4(direction, 1.f);
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
					.position = glm::vec4(posx, posy, posz, 1.f),
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
			lightSSBO[i].direction = object.direction;
			lightSSBO[i].position = object.position;
			lightSSBO[i].color_type = object.color_type;
		}
		});
}
