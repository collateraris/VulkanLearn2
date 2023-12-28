#pragma once

#include <vk_types.h>

#if AO_RAYTRACER_ON && GBUFFER_ON
#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

class VulkanAORaytracingGraphicsPipeline
{
public:
	struct alignas(16) GlobalAOParams
	{
		float aoRadius;
		uint32_t  frameCount;
		float minT;
		uint32_t  numRays;
	};
	VulkanAORaytracingGraphicsPipeline() = default;
	void init(VulkanEngine* engine, const std::array<Texture, 4>& gbuffer);
	void copy_global_uniform_data(VulkanAORaytracingGraphicsPipeline::GlobalAOParams& aoData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_output() const;

private:

	void create_blas(const std::vector<std::unique_ptr<Mesh>>& meshList);
	void create_tlas(const std::vector<RenderObject>& renderables);
	void init_description_set(const std::array<Texture, 4>& gbuffer);

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

	std::array<AllocatedBuffer, 2> _globalUniformsBuffer;
	AllocatedBuffer _objectBuffer;

	VkDescriptorSetLayout          _globalUniformsDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalUniformsDescSet;

	VkDescriptorSetLayout _gBuffDescSetLayout;
	std::array<VkDescriptorSet, 2> _gBuffDescSet;

	VulkanRaytracerBuilder _rtBuilder;
};

#endif

