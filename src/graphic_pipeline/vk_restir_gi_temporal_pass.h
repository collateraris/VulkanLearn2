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

class VulkanReSTIR_GI_TemporalPass
{
public:

	VulkanReSTIR_GI_TemporalPass() = default;
	void init(VulkanEngine* engine);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

private:

	Texture& get_tex(ETextureResourceNames name) const;

	void init_description_set_global_buffer();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat      _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };

	AllocatedBuffer                 _rtSBTBuffer;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager
};


