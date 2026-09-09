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

class VulkanReSTIR_DI_InitPass
{
public:

	VulkanReSTIR_DI_InitPass() = default;
	void init(VulkanEngine* engine);
	void draw(rhi::CommandList& cmd, int frameSlot);
	const AllocatedBuffer& shader_binding_table_buffer() const { return _rtSBTBuffer; }

	Texture& get_tex(ETextureResourceNames name) const;

private:

	void init_description_set_global_buffer();

	void init_tex();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat      _colorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };

	bool bResetAccumulation = false;

	AllocatedBuffer                 _rtSBTBuffer;

	std::array<AllocatedBuffer, 2> _globalUniformsBuffer;
	VkDescriptorSetLayout          _globalDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalDescSet;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager
};


