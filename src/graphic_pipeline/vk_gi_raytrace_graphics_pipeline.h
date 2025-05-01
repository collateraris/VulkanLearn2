#pragma once

#include <vk_types.h>


#include <vk_render_pass.h>
#include <vk_mesh.h>
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

class VulkanGIShadowsRaytracingGraphicsPipeline
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
		uint32_t mode = 0;
		uint32_t enableAccumulation = 0;
		uint32_t widthScreen = 0;
		uint32_t heightScreen = 0;
	};
	VulkanGIShadowsRaytracingGraphicsPipeline() = default;
	void init_textures(VulkanEngine* engine);
	void init(VulkanEngine* engine);
	void copy_global_uniform_data(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams& aoData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_output() const;

	void reset_accumulation();
	void try_reset_accumulation(PlayerCamera& camera);

private:

	void init_scene_descriptors();
	void init_global_buffers();

	VulkanEngine* _engine = nullptr;

	VkExtent3D _imageExtent;
	VkFormat    _colorFormat{ VK_FORMAT_R16G16B16A16_SFLOAT };
	VkFormat    _giSamplesColorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };

	std::array<AllocatedBuffer, 2> _globalUniformsBuffer;

	std::unique_ptr<VulkanReSTIRInitPass> _restirInitGP;
	std::unique_ptr<VulkanReSTIRTemporalPass> _restirTemporalGP;
	std::unique_ptr<VulkanReSTIRSpaceReusePass> _restirSpacialGP;
	std::unique_ptr<VulkanReSTIR_GI_TemporalPass> _restir_GI_TemporalGP;
	std::unique_ptr<VulkanReSTIR_GI_SpaceReusePass> _restir_GI_SpacialGP;
	std::unique_ptr<VulkanReSTIRUpdateReservoirPlusShadePass> _restirUpdateShadeGP;
	std::unique_ptr<VulkanRaytrace_ReflectionPass> _raytraceReflection;
	std::unique_ptr<VulkanSimpleAccumulationGraphicsPipeline> _accumulationGP;

	std::unique_ptr<VulkanRaytracerDenoiserPass> _denoiserPass;
};


