#pragma once

#include <vk_types.h>

#if GI_RAYTRACER_ON
#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

class VulkanReSTIRUpdateReservoirPlusShadePass
{
public:

	VulkanReSTIRUpdateReservoirPlusShadePass() = default;
	void init(VulkanEngine* engine);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_output() const;

	void barrier_for_frag_read(VulkanCommandBuffer* cmd);
	void barrier_for_raytrace_write(VulkanCommandBuffer* cmd);

private:
	void init_description_set_global_buffer();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	Texture _outputTex;
	VkFormat      _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };

	AllocatedBuffer                 _rtSBTBuffer;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager
};

#endif

