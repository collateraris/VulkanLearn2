#pragma once

#include <vk_types.h>

#if GI_RAYTRACER_ON && GBUFFER_ON
#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

class VulkanReSTIRInitPlusTemporalPass
{
public:

	VulkanReSTIRInitPlusTemporalPass() = default;
	void init(VulkanEngine* engine);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	void indirectOutput_barrier_for_raytrace_read(VulkanCommandBuffer* cmd);
	void indirectOutput_barrier_for_raytrace_write(VulkanCommandBuffer* cmd);
	void reservoirPrevTex_barrier_for_raytrace_read(VulkanCommandBuffer* cmd);
	void reservoirPrevTex_barrier_for_raytrace_write(VulkanCommandBuffer* cmd);
	void reservoirCurrTex_barrier_for_raytrace_read(VulkanCommandBuffer* cmd);
	void reservoirCurrTex_barrier_for_raytrace_write(VulkanCommandBuffer* cmd);

private:

	Texture& get_indirectOutput() const;
	Texture& get_reservoirCurrTex() const;
	Texture& get_reservoirPrevTex() const;

	void init_description_set_global_buffer();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat      _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };

	AllocatedBuffer                 _rtSBTBuffer;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	VkDescriptorSetLayout          _rtDescSetLayout;
	VkDescriptorSet  _rtDescSet;
};

#endif

