#include <graphic_pipeline/vk_restir_init_pass.h>
 

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <vk_utils.h>

void VulkanReSTIRInitPass::init(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		engine->_windowExtent.width,
		engine->_windowExtent.height,
		1
	};

	{
		init_tex();
		init_description_set_global_buffer(); 
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::ReSTIR_Init,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				uint32_t rayGenIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("restir_start.rgen.slang.spv")), VK_SHADER_STAGE_RAYGEN_BIT_NV);
				uint32_t rayIndirectMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("indirect_raytrace_gbuffer.rmiss.slang.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayShadowMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("shadow.rmiss.slang.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayIndirectClosestHitIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("indirect_raytrace_gbuffer.rchit.slang.spv")), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
				uint32_t rayIndirectAnyHitIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("indirect_raytrace_gbuffer.rahit.slang.spv")), VK_SHADER_STAGE_ANY_HIT_BIT_NV);
				uint32_t rayShadowAnyHitIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("shadow.rahit.slang.spv")), VK_SHADER_STAGE_ANY_HIT_BIT_NV);

				defaultEffect.reflect_layout(engine->_device, nullptr, 0);

				RTPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->setLayout,
																 _engine->get_engine_descriptor(EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0)->setLayout,
																 _rpDescrMan.get_layout() };
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
				group.generalShader = rayShadowMissIndex;
				rtShaderGroups.push_back(group);

				// closest hit shader
				// Payload 0
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				group.generalShader = VK_SHADER_UNUSED_KHR;
				group.closestHitShader = rayIndirectClosestHitIndex;
				group.anyHitShader = rayIndirectAnyHitIndex;
				rtShaderGroups.push_back(group);

				// Payload 1
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				group.generalShader = VK_SHADER_UNUSED_KHR;
				group.closestHitShader = VK_SHADER_UNUSED_KHR;
				group.anyHitShader = rayShadowAnyHitIndex;
				rtShaderGroups.push_back(group);

				pipelineBuilder._rayPipelineInfo.groupCount = static_cast<uint32_t>(rtShaderGroups.size());
				pipelineBuilder._rayPipelineInfo.pGroups = rtShaderGroups.data();

				pipelineLayout = pipelineBuilder._pipelineLayout;

				pipeline = pipelineBuilder.build_rt_pipeline(engine->_device);
			});
	}

	_rtSBTBuffer = VulkanRaytracerBuilder::create_SBTBuffer(engine, 2, 2, EPipelineType::ReSTIR_Init,
		_rgenRegion,
		_missRegion,
		_hitRegion,
		_callRegion);

}

Texture& VulkanReSTIRInitPass::get_tex(ETextureResourceNames name) const
{
	return *_engine->get_engine_texture(name);
}


void VulkanReSTIRInitPass::init_description_set_global_buffer()
{
	_rpDescrMan = vkutil::DescriptorManagerBuilder::begin(_engine, _engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_image(0, ETextureResourceNames::PT_REFERENCE_OUTPUT, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(1, ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(2, ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(3, ETextureResourceNames::PT_GBUFFER_NORMAL, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(4, ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.create_desciptor_manager();
}

void VulkanReSTIRInitPass::init_tex()
{
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::PT_REFERENCE_OUTPUT);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *_engine->_resManager.get_engine_texture(ETextureResourceNames::PT_REFERENCE_OUTPUT);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *_engine->_resManager.get_engine_texture(ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *_engine->_resManager.get_engine_texture(ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::PT_GBUFFER_NORMAL);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *_engine->_resManager.get_engine_texture(ETextureResourceNames::PT_GBUFFER_NORMAL);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *_engine->_resManager.get_engine_texture(ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}
}

void VulkanReSTIRInitPass::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	cmd->raytrace(&_rgenRegion, &_missRegion, &_hitRegion, &_callRegion, _imageExtent.width, _imageExtent.height, 1,
		[&](VkCommandBuffer cmd) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipeline(EPipelineType::ReSTIR_Init));
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_Init), 0,
				1, &_engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->set, 0, nullptr);

			EDescriptorResourceNames currentGlobalUniformsDesc = current_frame_index % 2 == 0
				? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0
				: EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_Init), 1, 1, &_engine->get_engine_descriptor(currentGlobalUniformsDesc)->set, 0, nullptr);

			_rpDescrMan.bind_descriptor_set(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_Init), 2);
		});
}


