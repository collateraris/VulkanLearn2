#pragma once

#include <vk_types.h>

#if VBUFFER_ON

#include <vk_mesh.h>

class VulkanEngine;
class VulkanCommandBuffer;
class RenderObject;

class VulkanVbufferShadingGraphicsPipeline
{
	struct alignas(16) ObjectData
	{
		glm::mat4 model;
		uint32_t meshIndex;
		uint32_t diffuseTexIndex;
		uint32_t pad1;
		uint32_t pad2;
	};
public:

	struct alignas(16) SGlobalCamera
	{
		glm::mat4 viewProj;     // Camera view * projection
	};

	void init(VulkanEngine* engine);
	void init_description_set(const Texture& vbuffer);
	void copy_global_uniform_data(VulkanVbufferShadingGraphicsPipeline::SGlobalCamera& camData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

private:
	void init_scene_buffer(const std::vector<RenderObject>& renderables, const std::vector<std::unique_ptr<Mesh>>& meshList);
	void init_bindless(const std::vector<std::unique_ptr<Mesh>>& meshList, const std::vector<Texture*>& textureList);

	VulkanEngine* _engine;

	VkDescriptorSetLayout          _descSetLayout;
	std::array<VkDescriptorSet, 2>  _descSet;

	VkDescriptorSet _bindlessSet;
	VkDescriptorSetLayout _bindlessSetLayout;

	std::array<AllocatedBuffer, 2> _globalCameraBuffer;
	AllocatedBuffer _objectBuffer;

	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;
};

#endif