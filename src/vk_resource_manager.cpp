#include <vk_resource_manager.h>
#include <vk_initializers.h>

uint32_t ResourceManager::store_texture(std::string& name)
{
	auto texFound = textureCache.find(name);
	if (texFound == textureCache.end())
	{
		textureCache[name] = std::make_unique<Texture>();
	}

	textureList.push_back(textureCache[name].get());

	return textureList.size() - 1;
}

AllocatedSampler* ResourceManager::create_engine_sampler(ESamplerType sampleNameId)
{
	if (engineSamplerCache[(uint32_t)sampleNameId] == nullptr)
		engineSamplerCache[(uint32_t)sampleNameId] = std::make_unique<AllocatedSampler>();

	return get_engine_sampler(sampleNameId);
}

AllocatedSampler* ResourceManager::get_engine_sampler(ESamplerType sampleNameId)
{
	return engineSamplerCache[(uint32_t)sampleNameId].get();
}


Texture* ResourceManager::create_engine_texture(ETextureResourceNames texNameId)
{
	if (engineTextureCache[(uint32_t)texNameId] == nullptr)
		engineTextureCache[(uint32_t)texNameId] = std::make_unique<Texture>();

	return get_engine_texture(texNameId);
}

Texture* ResourceManager::get_engine_texture(ETextureResourceNames texNameId)
{
	return engineTextureCache[(uint32_t)texNameId].get();
}

AllocateDescriptor* ResourceManager::create_engine_descriptor(EDescriptorResourceNames descrNameId)
{
	if (engineDescriptorCache[(uint32_t)descrNameId] == nullptr)
		engineDescriptorCache[(uint32_t)descrNameId] = std::make_unique<AllocateDescriptor>();

	return get_engine_descriptor(descrNameId);
}

AllocateDescriptor* ResourceManager::get_engine_descriptor(EDescriptorResourceNames descrNameId)
{
	return engineDescriptorCache[(uint32_t)descrNameId].get();
}
