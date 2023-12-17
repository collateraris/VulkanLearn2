#pragma once

#include <vk_types.h>

class VulkanEngine;
class VulkanCommandBuffer;

class VulkanFullScreenGraphicsPipeline
{
public:

	void init(VulkanEngine* engine);
	void init_description_set(const Texture& texture);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

private:

	VulkanEngine* _engine;

	VkDescriptorSetLayout          _descSetLayout;
	std::array<VkDescriptorSet, 2>  _descSet;
};