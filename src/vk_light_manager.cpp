#include <vk_light_manager.h>

#include <vk_engine.h>

void VulkanLightManager::init(VulkanEngine* engine)
{
	_engine = engine;

	get_sun_light();
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

VulkanLightManager::Light* VulkanLightManager::get_sun_light()
{
	if (_lightsOnScene.empty())
	{
		add_light({ .type = static_cast<uint32_t>(ELightType::Sun) });
		for (int i = 0; i < FRAME_OVERLAP; i++)
			create_light_buffer(i);
	}

	//fist index is sun
	return _lightsOnScene[0].get();
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
