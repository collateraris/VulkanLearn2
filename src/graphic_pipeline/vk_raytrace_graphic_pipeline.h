#pragma once

#include <vk_types.h>

#if RAYTRACER_ON
#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;

class VulkanRaytracingGraphicsPipeline
{
	struct alignas(16) ObjectData
	{
		uint32_t meshIndex;
		uint32_t diffuseTexIndex;
		uint32_t pad1;
		uint32_t pad2;
	};
public:
	struct alignas(16) GlobalUniforms
	{
		glm::mat4 viewProj;     // Camera view * projection
		glm::mat4 viewInverse;  // Camera inverse view matrix
		glm::mat4 projInverse;  // Camera inverse projection matrix
	};
	VulkanRaytracingGraphicsPipeline() = default;
	void init(VulkanEngine* engine);
	void copy_global_uniform_data(VulkanRaytracingGraphicsPipeline::GlobalUniforms& camData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_output() const;

private:

	void create_blas(const std::vector<std::unique_ptr<Mesh>>& meshList);
	void create_tlas(const std::vector<RenderObject>& renderables);
	void init_bindless(const std::vector<std::unique_ptr<Mesh>>& meshList, const std::vector<Texture*>& textureList);

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

	std::array<AllocatedBuffer,2> _globalUniformsBuffer;
	AllocatedBuffer _objectBuffer;

	VkDescriptorSetLayout          _globalUniformsDescSetLayout;
	std::array<VkDescriptorSet, 2>  _globalUniformsDescSet;

	VkDescriptorSet _bindlessSet;
	VkDescriptorSetLayout _bindlessSetLayout;

	VulkanRaytracerBuilder _rtBuilder;

	RenderPassInfo _rp_info = {};
	RenderPassInfo::Subpass _subpass = {};
};

#endif

