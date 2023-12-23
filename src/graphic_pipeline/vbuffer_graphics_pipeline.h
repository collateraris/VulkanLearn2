#pragma once

#include <vk_types.h>

#if VBUFFER_ON

#include <vk_mesh.h>

class VulkanEngine;
class VulkanCommandBuffer;
class RenderObject;

class VulkanVbufferGraphicsPipeline
{
	struct alignas(16) ObjectData
	{
		glm::mat4 model;
		uint32_t meshletCount;
		uint32_t meshIndex;
		uint32_t pad1;
		uint32_t pad2;
	};
public:

	struct alignas(16) SGlobalCamera
	{
		glm::mat4 viewProj;     // Camera view * projection
	};

	VulkanVbufferGraphicsPipeline() = default;
	void init(VulkanEngine* engine);
	void copy_global_uniform_data(VulkanVbufferGraphicsPipeline::SGlobalCamera& camData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_vbuffer_output() const;

private:

	void init_scene_buffer(const std::vector<RenderObject>& renderables, const std::vector<std::unique_ptr<Mesh>>& meshList);
	void init_bindless(const std::vector<std::unique_ptr<Mesh>>& meshList);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _vbufferExtent;
	Texture _vbufferTex;
	VkFormat      _vbufferFormat{ VK_FORMAT_R16_UINT };
	Texture _depthTexture;
	VkFormat      _depthFormat{ VK_FORMAT_D32_SFLOAT };

	AllocatedBuffer _indirectBuffer;

	std::array<AllocatedBuffer, 2> _globalCameraBuffer;
	AllocatedBuffer _objectBuffer;
	uint32_t _objectsSize = 0;

	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;

	VkDescriptorSet _bindlessSet;
	VkDescriptorSetLayout _bindlessSetLayout;

	VkDescriptorSetLayout          _vbufferDescSetLayout;
	std::array<VkDescriptorSet, 2>  _vbufferDescSet;
};

#endif