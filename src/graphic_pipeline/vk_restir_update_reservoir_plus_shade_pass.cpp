#include <graphic_pipeline/vk_restir_update_reservoir_plus_shade_pass.h>

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>

void VulkanReSTIRUpdateReservoirPlusShadePass::init(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		_engine->_windowExtent.width,
		_engine->_windowExtent.height,
		1
	};

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_outputTex = texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}

	{
		init_description_set_global_buffer();
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::ReSTIR_UpdateReservoir_PlusShade,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				uint32_t rayGenIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("reSTIR_updateReservoir_PlusShade.rgen.spv")), VK_SHADER_STAGE_RAYGEN_BIT_NV);
				uint32_t rayIndirectMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("indirect_raytrace.rmiss.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("ao_raytrace.rmiss.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayIndirectClosestHitIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("indirect_raytrace.rchit.spv")), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

				defaultEffect.reflect_layout(engine->_device, nullptr, 0);

				RTPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->setLayout,
																 _engine->get_engine_descriptor(EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0)->setLayout,
																 _rpDescrMan.get_layout()};
				mesh_pipeline_layout_info.setLayoutCount = setLayout.size();
				mesh_pipeline_layout_info.pSetLayouts = setLayout.data();

				vkCreatePipelineLayout(_engine->_device, &mesh_pipeline_layout_info, nullptr, &pipelineBuilder._pipelineLayout);

				// The ray tracing process can shoot rays from the camera, and a shadow ray can be shot from the
				// hit points of the camera rays, hence a recursion level of 2. This number should be kept as low
				// as possible for performance reasons. Even recursive ray tracing should be flattened into a loop
				// in the ray generation to avoid deep recursion.
				pipelineBuilder._rayPipelineInfo.maxPipelineRayRecursionDepth = 2;  // Ray depth

				// Shader groups
				std::vector<VkRayTracingShaderGroupCreateInfoKHR> rtShaderGroups;
				VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
				group.anyHitShader = VK_SHADER_UNUSED_KHR;
				group.closestHitShader = VK_SHADER_UNUSED_KHR;
				group.generalShader = VK_SHADER_UNUSED_KHR;
				group.intersectionShader = VK_SHADER_UNUSED_KHR;

				// Raygen
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				group.generalShader = rayGenIndex;
				rtShaderGroups.push_back(group);

				// Miss
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				group.generalShader = rayIndirectMissIndex;
				rtShaderGroups.push_back(group);

				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				group.generalShader = rayMissIndex;
				rtShaderGroups.push_back(group);

				// closest hit shader
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				group.generalShader = VK_SHADER_UNUSED_KHR;
				group.closestHitShader = rayIndirectClosestHitIndex;
				rtShaderGroups.push_back(group);

				pipelineBuilder._rayPipelineInfo.groupCount = static_cast<uint32_t>(rtShaderGroups.size());
				pipelineBuilder._rayPipelineInfo.pGroups = rtShaderGroups.data();

				pipelineLayout = pipelineBuilder._pipelineLayout;

				pipeline = pipelineBuilder.build_rt_pipeline(engine->_device);
			});
	}

	_rtSBTBuffer = VulkanRaytracerBuilder::create_SBTBuffer(engine, 2, 1, EPipelineType::ReSTIR_UpdateReservoir_PlusShade,
		_rgenRegion,
		_missRegion,
		_hitRegion,
		_callRegion);
}

void VulkanReSTIRUpdateReservoirPlusShadePass::init_description_set_global_buffer()
{
	_rpDescrMan = vkutil::DescriptorManagerBuilder::begin(_engine, _engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_image(0, ETextureResourceNames::ReSTIR_DI_SPACIAL_RESERVOIRS, EResOp::READ_STORAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(1, ETextureResourceNames::ReSTIR_DI_PREV_RESERVOIRS, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(2, _outputTex, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)

		.bind_image(3, ETextureResourceNames::ReSTIR_GI_SPACIAL_RESERVOIRS, EResOp::READ_STORAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(4, ETextureResourceNames::ReSTIR_INDIRECT_LO_SPACIAL, EResOp::READ_STORAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(5, ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_SPACIAL, EResOp::READ_STORAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(6, ETextureResourceNames::ReSTIR_GI_SAMPLES_NORMAL_SPACIAL, EResOp::READ_STORAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)

		.bind_image(7, ETextureResourceNames::ReSTIR_GI_PREV_RESERVOIRS, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(8, ETextureResourceNames::ReSTIR_INDIRECT_LO_PREV, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(9, ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_PREV, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(10, ETextureResourceNames::ReSTIR_GI_SAMPLES_NORMAL_PREV, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)

		.bind_image(11, ETextureResourceNames::RAYTRACE_REFLECTION, EResOp::READ_STORAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)

		.create_desciptor_manager();
}

void VulkanReSTIRUpdateReservoirPlusShadePass::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	VkClearValue clear_value = { 0., 0., 0., 0. };
	cmd->clear_image(_outputTex, clear_value);

	vkutil::image_pipeline_barrier(cmd->get_cmd(), _outputTex, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

	cmd->raytrace(&_rgenRegion, &_missRegion, &_hitRegion, &_callRegion, _imageExtent.width, _imageExtent.height, 1,
		[&](VkCommandBuffer cmd) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipeline(EPipelineType::ReSTIR_UpdateReservoir_PlusShade));
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_UpdateReservoir_PlusShade), 0,
				1, &_engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->set, 0, nullptr);

			EDescriptorResourceNames currentGlobalUniformsDesc = current_frame_index % 2 == 0
				? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0
				: EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_UpdateReservoir_PlusShade), 1,
				1, &_engine->get_engine_descriptor(currentGlobalUniformsDesc)->set, 0, nullptr);

			_rpDescrMan.bind_descriptor_set(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_UpdateReservoir_PlusShade), 2);
		});
}

const Texture& VulkanReSTIRUpdateReservoirPlusShadePass::get_output() const
{
	return _outputTex;
}

void VulkanReSTIRUpdateReservoirPlusShadePass::barrier_for_frag_read(VulkanCommandBuffer* cmd)
{
	vkutil::image_pipeline_barrier(cmd->get_cmd(), _outputTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VulkanReSTIRUpdateReservoirPlusShadePass::barrier_for_raytrace_write(VulkanCommandBuffer* cmd)
{
	vkutil::image_pipeline_barrier(cmd->get_cmd(), _outputTex, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
}
