#pragma once

#include <vk_types.h>

#include <vk_render_pass.h>
#include <vk_mesh.h>
#include <vk_descriptors.h>
#include <graphic_pipeline/vk_restir_init_pass.h>
#include <graphic_pipeline/vk_restir_temporal_pass.h>
#include <graphic_pipeline/vk_restir_spacial_reuse_pass.h>
#include <graphic_pipeline/vk_restir_gi_temporal_pass.h>
#include <graphic_pipeline/vk_restir_gi_spacial_reuse_pass.h>
#include <graphic_pipeline/vk_restir_update_reservoir_plus_shade_pass.h>
#include <graphic_pipeline/vk_raytrace_reflection.h>
#include <graphic_pipeline/vk_simple_accumulation_graphics_pipeline.h>
#include <graphic_pipeline/vk_raytracer_denoiser_pass.h>

class VulkanEngine;
class VulkanFrameBuffer;
class VulkanCommandBuffer;
class RenderObject;
struct PlayerCamera;

class VulkanReSTIRPathtracingGraphicsPipeline
{
public:
	struct alignas(16) GlobalGIParams
	{
		glm::mat4 projView;
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
		glm::mat4 prevProjView;
		glm::vec4 camPos;
		uint32_t  frameCount;
		float shadowMult;
		uint32_t lightsCount;
		uint32_t  numRays;
		int32_t sunIndex = -1;
		uint32_t enableAccumulation = 0;
		uint32_t widthScreen = 0;
		uint32_t heightScreen = 0;
		glm::vec4 gridMax = glm::vec4(1);
		glm::vec4 gridMin = glm::vec4(1);
	};
	VulkanReSTIRPathtracingGraphicsPipeline() = default;
	void init_textures(VulkanEngine* engine);
	void init(VulkanEngine* engine);
	void copy_global_uniform_data(VulkanReSTIRPathtracingGraphicsPipeline::GlobalGIParams& aoData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	void barrier_for_frag_read(VulkanCommandBuffer* cmd);
	const Texture& get_output() const;

private:

	void init_scene_descriptors();
	void init_global_buffers();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat    _colorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };

	std::array<AllocatedBuffer, 2> _globalUniformsBuffer;

	AllocatedBuffer                 _rtSBTBuffer;

	VkStridedDeviceAddressRegionKHR _rgenRegion{};
	VkStridedDeviceAddressRegionKHR _missRegion{};
	VkStridedDeviceAddressRegionKHR _hitRegion{};
	VkStridedDeviceAddressRegionKHR _callRegion{};

	vkutil::DescriptorManager _rpDescrMan; // render pass descriptor manager
};

