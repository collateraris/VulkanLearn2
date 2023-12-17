#pragma once

#include <vk_types.h>
#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

class VulkanRaytracingGraphicsPipeline
{
public:
	VulkanRaytracingGraphicsPipeline() = default;
	void init(VulkanEngine* engine);
	void create_blas(const std::vector<std::unique_ptr<Mesh>>& meshList);
	void create_tlas(const std::vector<RenderObject>& renderables);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

private:

	VulkanRaytracerBuilder::BlasInput create_blas_input(Mesh& mesh);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	Texture _colorTexture;
	VkFormat      _colorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };
	Texture _depthTexture;
	VkFormat      _depthFormat{ VK_FORMAT_D32_SFLOAT };

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR _rtProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

	AllocatedBuffer                 _rtSBTBuffer;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	VkDescriptorSetLayout          _rtDescSetLayout;
	std::array<VkDescriptorSet, 2>  _rtDescSet;

	VulkanRaytracerBuilder _rtBuilder;

	RenderPassInfo _rp_info = {};
	RenderPassInfo::Subpass _subpass = {};
};

