#pragma once

#include <vk_types.h>
#include <rhi/rhi.h>

#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>

class VulkanEngine;
class VulkanFrameBuffer;
class RenderObject;

class VulkanReSTIR_PT_SpaceReusePass
{
public:

	VulkanReSTIR_PT_SpaceReusePass() = default;
	void init(VulkanEngine* engine);
	void draw(rhi::CommandList& cmd, int frameSlot);
	const AllocatedBuffer& shader_binding_table_buffer() const { return _rtSBTBuffer; }

private:
	Texture& get_tex(ETextureResourceNames name) const;

	void init_description_set_global_buffer();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	const uint32_t _tileSize = 16;
	uint32_t _tileNumberWidth;
	uint32_t _tileNumberHeight;

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	AllocatedBuffer                 _rtSBTBuffer;
};

