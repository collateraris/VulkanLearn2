#pragma once

#include <vk_types.h>
#include <vk_mesh.h>

struct MaterialDesc
{
	std::string matName = {};
	std::string diffuseTexture = {};
	uint32_t diffuseTextureIndex;
	std::string normalTexture = {};
	int32_t normalTextureIndex = -1;
	std::string metalnessTexture = {};
	int32_t metalnessTextureIndex = -1;
	std::string roughnessTexture = {};
	int32_t roughnessTextureIndex = -1;
	std::string emissionTexture = {};
	int32_t emissionTextureIndex = -1;
	std::string opacityTexture = {};
	int32_t opacityTextureIndex = -1;
};

class ResourceManager
{
public:

	std::unordered_map<std::string, std::unique_ptr<Texture>> textureCache;
	std::vector<std::unique_ptr<Mesh>> meshList;
	std::vector<std::unique_ptr<MaterialDesc>> matDescList; 
	std::vector<Texture*> textureList;

	uint32_t storeTexture(std::string& name);
};