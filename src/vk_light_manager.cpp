#include <vk_light_manager.h>

#include <vk_engine.h>

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


void VulkanLightManager::create_light_buffer()
{
	assert(_lightsBuffer._buffer == VK_NULL_HANDLE);

	uint32_t bufferSize = _lightsOnScene.size();
	_lightsBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanLightManager::Light) * bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	update_light_buffer();
}

void VulkanLightManager::update_light_buffer()
{
	_engine->map_buffer(_engine->_allocator, _lightsBuffer._allocation, [&](void*& data) {
		VulkanLightManager::Light* lightSSBO = (VulkanLightManager::Light*)data;
			for (int i = 0; i < _lightsOnScene.size(); i++)
			{
				const VulkanLightManager::Light& object = *_lightsOnScene[i];
				lightSSBO[i].type = object.type;
				lightSSBO[i].direction = object.direction;
				lightSSBO[i].position = object.position;
			}
		});
}

const AllocatedBuffer& VulkanLightManager::get_light_buffer() const
{
	return _lightsBuffer;
}
