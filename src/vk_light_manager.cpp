#include <vk_light_manager.h>

#include <vk_engine.h>
#include <time.h>

glm::vec3 randomColor()
{
	glm::vec3 color;
	for (int i = 0; i < 3; i++)
	{
		srand(time(NULL)); // Seed random number generator. Only do this once.

		float val = rand() % 256; // Set val equal to a random number between 0 and 6.

		color[i] = val /25.f;
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

VulkanLightManager::Light* VulkanLightManager::add_light(VulkanLightManager::Light&& lightInfo)
{
	assert(lightInfo.type != static_cast<uint32_t>(ELightType::None));

	_lightsOnScene.emplace_back(std::make_unique<Light>());

	Light* light = _lightsOnScene.back().get();
	light->direction = lightInfo.direction;
	light->position = lightInfo.position;
	light->type = lightInfo.type;
	// return index
	return light;
}

void VulkanLightManager::add_sun_light()
{
	add_light({ .type = static_cast<uint32_t>(ELightType::Sun) });
}

VulkanLightManager::Light* VulkanLightManager::get_sun_light()
{
	//fist index is sun
	return _lightsOnScene[0].get();
}

void VulkanLightManager::create_light_buffers()
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
		create_light_buffer(i);
}

void VulkanLightManager::create_light_buffer(int current_frame_index)
{
	assert(_lightsBuffer[current_frame_index]._buffer == VK_NULL_HANDLE);

	uint32_t bufferSize = VULKAN_MAX_LIGHT_COUNT;
	_lightsBuffer[current_frame_index] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanLightManager::Light) * bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	update_light_buffer(current_frame_index);
}

void VulkanLightManager::update_light_buffer(int current_frame_index)
{
	_engine->map_buffer(_engine->_allocator, _lightsBuffer[current_frame_index]._allocation, [&](void*& data) {
		VulkanLightManager::Light* lightSSBO = (VulkanLightManager::Light*)data;
			for (int i = 0; i < _lightsOnScene.size() && i < VULKAN_MAX_LIGHT_COUNT; i++)
			{
				const VulkanLightManager::Light& object = *_lightsOnScene[i];
				lightSSBO[i].type = object.type;
				lightSSBO[i].direction = object.direction;
				lightSSBO[i].position = object.position;
				lightSSBO[i].color = object.color;
			}
		});
}

const AllocatedBuffer& VulkanLightManager::get_light_buffer(int current_frame_index) const
{
	return _lightsBuffer[current_frame_index];
}

const std::vector<std::unique_ptr<VulkanLightManager::Light>>& VulkanLightManager::get_lights() const
{
	return _lightsOnScene;
}

void VulkanLightManager::generateUniformGrid(glm::vec3 maxCube, glm::vec3 minCube, uint32_t lightNumber)
{
	float stepX = std::abs(maxCube.x - minCube.x) / static_cast<float>(lightNumber);
	float stepY = std::abs(maxCube.y - minCube.y) / static_cast<float>(lightNumber);
	float stepZ = std::abs(maxCube.z - minCube.z) / static_cast<float>(lightNumber);

	for (float posx = minCube.x; posx < maxCube.x; posx+= stepX)
	{
		for (float posy = minCube.y; posy < maxCube.y; posy += stepY)
		{
			for (float posz = minCube.z; posz < maxCube.z; posz += stepZ)
			{
				_lightsOnScene.emplace_back(std::make_unique<Light>());

				Light* light = _lightsOnScene.back().get();
				light->position = glm::vec4(posx, posy, posz, 1.f);
				light->type = static_cast<uint32_t>(ELightType::Point);
				light->color = glm::vec4(randomColor(),1.f);
			}
		}
	}
}
