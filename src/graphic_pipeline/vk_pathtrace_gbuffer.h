#pragma once

#include <vk_types.h>

#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

class VulkanPTGBuffer
{
public:

	struct alignas(16) SGlobalRQParams
	{
		glm::mat4 proj_to_world_space;
		glm::mat4 world_to_proj_space;
		vec4 width_height_fov_frameIndex;
	};

	VulkanPTGBuffer() = default;
	void init(VulkanEngine* engine);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	void copy_global_uniform_data(VulkanPTGBuffer::SGlobalRQParams& rqData, int current_frame_index);

	void barrier_for_reading(VulkanCommandBuffer* cmd);
	void barrier_for_writing(VulkanCommandBuffer* cmd);

	Texture& get_tex(ETextureResourceNames name) const;

private:

	void init_description_set_global_buffer();

	void init_tex();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat      _colorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };

	AllocatedBuffer                 _rtSBTBuffer;

	std::array<AllocatedBuffer, 2> _globalRQBuffer;
	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager
};


