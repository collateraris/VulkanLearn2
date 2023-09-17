#pragma once

#include <vk_types.h>
#include <vk_mesh.h>

struct Texture {
	AllocatedImage image;
	VkImageView imageView;
};

struct MaterialDesc
{
	std::string diffuseTexture = {};
};

class ResourceManager
{
public:

	using meshIndex = int;
	using matIndex = int;

	std::unordered_map<std::string, std::unique_ptr<Texture>> textureCache;
	std::vector<std::unique_ptr<Mesh>> meshList;
	std::vector<std::unique_ptr<MaterialDesc>> matDescList;

};