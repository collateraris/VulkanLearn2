#pragma once

#include <vk_types.h>

#include <vk_mesh.h>

class VulkanEngine;
class VulkanCommandBuffer;
class RenderObject;

class VulkanGbufferShadingGraphicsPipeline
{
public:

	void init(VulkanEngine* engine, const Texture& gi);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

private:
	void init_description_set(const Texture& gi);

	VulkanEngine* _engine;

	VkDescriptorSetLayout          _gBufDescSetLayout;
	std::array<VkDescriptorSet, 2>  _gBufDescSet;
};