#pragma once

#include <vk_types.h>
#include <vk_mesh.h>

class VulkanEngine;
class VulkanCommandBuffer;
class RenderObject;

class VulkanLightsDebugGraphicsPipeline
{
public:

	void init(VulkanEngine* engine);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

private:

	void loadBoxMesh();

	VulkanEngine* _engine;

	VkDescriptorSetLayout          _descSetLayout;
	std::array<VkDescriptorSet, 2>  _descSet;

	VkDescriptorSet _bindlessSet;
	VkDescriptorSetLayout _bindlessSetLayout;

	std::array<AllocatedBuffer, 2> _globalCameraBuffer;
	AllocatedBuffer _objectBuffer;

	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;

	AllocatedBuffer _boxVB;
};