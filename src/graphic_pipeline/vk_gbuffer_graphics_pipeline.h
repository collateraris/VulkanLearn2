#pragma once

#include <vk_types.h>

#if GBUFFER_ON

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
	const std::array<Texture, 4> get_gbuffer() const;

private:

	void init_gbuffer_tex();
	void init_render_pass();
	void init_scene_buffer(const std::vector<RenderObject>& renderables, const std::vector<std::unique_ptr<Mesh>>& meshList);
	void init_bindless(const std::vector<std::unique_ptr<Mesh>>& meshList);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _texExtent;

	std::array<Texture, 4> _gbuffer;

	VkFormat      _wposFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };
	VkFormat      _normalFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };
	VkFormat      _uvFormat{ VK_FORMAT_R16G16_SFLOAT };
	VkFormat      _objIDFormat{ VK_FORMAT_R32_SFLOAT };

	Texture _depthTexture;
	VkFormat      _depthFormat{ VK_FORMAT_D32_SFLOAT };

	VkFramebuffer _gBufFramebuffer;

	AllocatedBuffer _indirectBuffer;

	std::array<AllocatedBuffer, 2> _globalCameraBuffer;
	AllocatedBuffer _objectBuffer;
	uint32_t _objectsSize = 0;

	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;

	VkDescriptorSet _bindlessSet;
	VkDescriptorSetLayout _bindlessSetLayout;
};

#endif