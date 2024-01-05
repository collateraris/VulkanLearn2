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

class VulkanGIShadowsRaytracingGraphicsPipeline
{
	struct alignas(16) ObjectData
	{
		uint32_t meshIndex;
		uint32_t diffuseTexIndex;
		int32_t normalTexIndex = -1;
		int32_t metalnessTexIndex = -1;
		int32_t roughnessTexIndex = -1;
		int32_t emissionTexIndex = -1;
		int32_t opacityTexIndex = -1;
		uint32_t pad;
	};
public:
	struct alignas(16) GlobalGIParams
	{
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
		glm::vec4 camPos;
		float aoRadius;
		uint32_t  frameCount;
		float shadowMult;
		uint32_t  numRays;
	};
	VulkanGIShadowsRaytracingGraphicsPipeline() = default;
	void init(VulkanEngine* engine, const std::array<Texture, 4>& gbuffer, const Texture& envMap);
	void copy_global_uniform_data(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams& aoData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_output() const;

	void barrier_for_frag_read(VulkanCommandBuffer* cmd);
	void barrier_for_gi_raytracing(VulkanCommandBuffer* cmd);

private:

	void create_blas(const std::vector<std::unique_ptr<Mesh>>& meshList);
	void create_tlas(const std::vector<RenderObject>& renderables);
	void init_description_set(const std::array<Texture, 4>& gbuffer, const Texture& envMap);
	void init_bindless(const std::vector<std::unique_ptr<Mesh>>& meshList, const std::vector<Texture*>& textureList);

	VulkanRaytracerBuilder::BlasInput create_blas_input(Mesh& mesh);

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	Texture _colorTexture;
	VkFormat      _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };
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

	VkDescriptorSet _bindlessSet;
	VkDescriptorSetLayout _bindlessSetLayout;

	VulkanRaytracerBuilder _rtBuilder;
};

#endif

