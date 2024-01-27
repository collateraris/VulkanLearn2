#include <vk_resource_manager.h>

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
