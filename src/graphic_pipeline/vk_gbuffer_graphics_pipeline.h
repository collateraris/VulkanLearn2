#pragma once

#include <vk_types.h>

#include <vk_mesh.h>

class VulkanEngine;
class VulkanCommandBuffer;
class RenderObject;

class VulkanGbufferGenerateGraphicsPipeline
{
	struct alignas(16) ObjectData
	{
		glm::mat4 model;
		uint32_t meshIndex;
		uint32_t meshletCount;
		uint32_t pad1;
		uint32_t pad2;
	};
public:

	struct alignas(16) SGlobalCamera
	{
		glm::mat4 viewProj;     // Camera view * projection
	};

	VulkanGbufferGenerateGraphicsPipeline() = default;
	void init(VulkanEngine* engine);
	void copy_global_uniform_data(VulkanGbufferGenerateGraphicsPipeline::SGlobalCamera& camData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	void barrier_for_gbuffer_shading(VulkanCommandBuffer* cmd);
	void barrier_for_gbuffer_generate(VulkanCommandBuffer* cmd);

	const Texture& get_wpos_output() const;
	const Texture& get_normal_output() const;
	const Texture& get_uv_output() const;
	const Texture& get_objID_output() const;
	const Texture& get_depth_output() const;

private:

	void init_gbuffer_tex();
	void init_render_pass();
	void init_scene_buffer(const std::vector<RenderObject>& renderables);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _texExtent;

	VkFormat      _wposFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };
	VkFormat      _normalFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };
	VkFormat      _uvFormat{ VK_FORMAT_R16G16_SFLOAT };
	VkFormat      _objIDFormat{ VK_FORMAT_R32_SFLOAT };
	VkFormat      _depthFormat{ VK_FORMAT_D32_SFLOAT };

	VkFramebuffer _gBufFramebuffer;

	AllocatedBuffer _indirectBuffer;

	std::array<AllocatedBuffer, 2> _globalCameraBuffer;
	AllocatedBuffer _objectBuffer;
	uint32_t _objectsSize = 0;

	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;
};