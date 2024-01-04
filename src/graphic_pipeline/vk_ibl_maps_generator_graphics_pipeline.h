#pragma once

#include <vk_types.h>
class VulkanEngine;
class VulkanIblMapsGeneratorGraphicsPipeline
{
public:
	VulkanIblMapsGeneratorGraphicsPipeline() = default;
	void init(VulkanEngine* engine, std::string hdrCubemapPath);

	const Texture& getHDR() const;
	const Texture& getEnvCubemap() const;
private:
	void loadEnvironment(std::string filename);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat   _colorFormat{ VK_FORMAT_R16G16B16_SFLOAT };

	Texture _hdr;
	Texture _environmentCube;
	Texture _lutBrdf;
	Texture _irradianceCube;
	Texture _prefilteredCube;


};