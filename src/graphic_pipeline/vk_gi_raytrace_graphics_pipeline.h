#pragma once

#include <vk_types.h>

#if GI_RAYTRACER_ON
#include <vk_raytracer_builder.h>
#include <vk_render_pass.h>
#include <vk_mesh.h>
#include <graphic_pipeline/vk_restir_init_plus_temporal_pass.h>
#include <graphic_pipeline/vk_restir_spacial_reuse_pass.h>
#include <graphic_pipeline/vk_restir_update_reservoir_plus_shade_pass.h>
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
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
		glm::mat4 prevProjView;
		glm::vec4 camPos;
		uint32_t  frameCount;
		float shadowMult;
		uint32_t lightsCount;
		uint32_t  numRays;
	};
	VulkanGIShadowsRaytracingGraphicsPipeline() = default;
	void init(VulkanEngine* engine);
	void copy_global_uniform_data(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams& aoData, int current_frame_index);
	void draw(VulkanCommandBuffer* cmd, int current_frame_index);

	const Texture& get_output() const;

	void reset_accumulation();
	void try_reset_accumulation(PlayerCamera& camera);

private:

	void create_blas(const std::vector<std::unique_ptr<Mesh>>& meshList);
	void create_tlas(const std::vector<RenderObject>& renderables);
	void init_scene_descriptors(const std::vector<std::unique_ptr<Mesh>>& meshList, const std::vector<Texture*>& textureList, VkAccelerationStructureKHR  tlas);
	void init_global_buffers();

	VulkanRaytracerBuilder::BlasInput create_blas_input(Mesh& mesh);

	VulkanEngine* _engine = nullptr;

	std::array<AllocatedBuffer, 2> _globalUniformsBuffer;

	VulkanRaytracerBuilder _rtBuilder;

	std::unique_ptr<VulkanReSTIRInitPlusTemporalPass> _restirInitTemporalGP;
	std::unique_ptr<VulkanReSTIRSpaceReusePass> _restirSpacialGP;
	std::unique_ptr<VulkanReSTIRUpdateReservoirPlusShadePass> _restirUpdateShadeGP;
	std::unique_ptr<VulkanSimpleAccumulationGraphicsPipeline> _accumulationGP;

	std::unique_ptr<VulkanRaytracerDenoiserPass> _denoiserPass;
};

#endif

