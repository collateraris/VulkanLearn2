#pragma once

#include <vk_types.h>

#include <vk_mesh.h>

class VulkanEngine;
class VulkanCommandBuffer;
class RenderObject;

class VulkanPathTracerGraphicsPipeline
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

	struct alignas(16) SGlobalRQParams
	{
		glm::mat4 proj_to_world_space;
		glm::mat4 world_to_proj_space;
		vec4 width_height_fov_frameIndex;
	};

	VulkanPathTracerGraphicsPipeline() = default;
	void init(VulkanEngine* engine);
	void copy_global_uniform_data(VulkanPathTracerGraphicsPipeline::SGlobalCamera& camData, int current_frame_index);
	void copy_global_uniform_data(VulkanPathTracerGraphicsPipeline::SGlobalRQParams& rqData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	void barrier_for_reading(VulkanCommandBuffer* cmd);
	void barrier_for_writing(VulkanCommandBuffer* cmd);

	const Texture& get_output() const;

private:

	void init_tex();
	void init_render_pass();
	void init_scene_buffer(const std::vector<RenderObject>& renderables);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _texExtent;

	VkFormat      _colorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };
	VkFormat      _depthFormat{ VK_FORMAT_D32_SFLOAT };

	VkFramebuffer _framebuffer;

	AllocatedBuffer _indirectBuffer;

	std::array<AllocatedBuffer, 2> _globalCameraBuffer;
	std::array<AllocatedBuffer, 2> _globalRQBuffer;
	AllocatedBuffer _objectBuffer;
	uint32_t _objectsSize = 0;

	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;
};