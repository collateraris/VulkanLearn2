#pragma once

#include <vk_types.h>
class VulkanEngine;
class VulkanIblMapsGeneratorGraphicsPipeline
{
public:
	VulkanIblMapsGeneratorGraphicsPipeline() = default;
	void init(VulkanEngine* engine, std::string hdrCubemapPath);

	const Texture& getHDR() const;
private:
	void loadEnvironment(std::string filename);

	VulkanEngine* _engine = nullptr;

	Texture _hdr;
	Texture _environmentCube;
	Texture _lutBrdf;
	Texture _irradianceCube;
	Texture _prefilteredCube;
};