#include <graphic_pipeline/vk_restir_pathtrace_graphics_pipeline.h>

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <vk_camera.h>

void VulkanReSTIRPathtracingGraphicsPipeline::init_textures(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		engine->_windowExtent.width,
		engine->_windowExtent.height,
		1
	};

	VulkanTextureBuilder texBuilder;
	texBuilder.init(_engine);
	texBuilder.start()
		.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
		.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
		.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
		.create_engine_texture(ETextureResourceNames::ReSTIR_PT_OUTPUT);
}

void VulkanReSTIRPathtracingGraphicsPipeline::init(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		engine->_windowExtent.width,
		engine->_windowExtent.height,
		1
	};

	{
		init_global_buffers();
		init_scene_descriptors();
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::ReSTIR_PT,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				uint32_t rayGenIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("reSTIR_PT.rgen.spv")), VK_SHADER_STAGE_RAYGEN_BIT_NV);
				uint32_t rayIndirectMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("indirect_raytrace.rmiss.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("ao_raytrace.rmiss.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayIndirectClosestHitIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("indirect_raytrace.rchit.spv")), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

				defaultEffect.reflect_layout(engine->_device, nullptr, 0);

				RTPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->setLayout,
																 _engine->get_engine_descriptor(EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0)->setLayout,
																  _engine->get_engine_descriptor(EDescriptorResourceNames::GBuffer)->setLayout,
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

	_rtSBTBuffer = VulkanRaytracerBuilder::create_SBTBuffer(engine, 2, 1, EPipelineType::ReSTIR_PT,
		_rgenRegion,
		_missRegion,
		_hitRegion,
		_callRegion);
}


void VulkanReSTIRPathtracingGraphicsPipeline::init_scene_descriptors()
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		//set 1

		VkDescriptorBufferInfo globalUniformsInfo;
		globalUniformsInfo.buffer = _globalUniformsBuffer[i]._buffer;
		globalUniformsInfo.offset = 0;
		globalUniformsInfo.range = VK_WHOLE_SIZE;

		EDescriptorResourceNames currentDesciptor = i == 0 ? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0 : EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
			.build(_engine, currentDesciptor);
	}

	_rpDescrMan = vkutil::DescriptorManagerBuilder::begin(_engine, _engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_image(0, ETextureResourceNames::ReSTIR_PT_OUTPUT, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.create_desciptor_manager();
}

void VulkanReSTIRPathtracingGraphicsPipeline::init_global_buffers()
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_globalUniformsBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanReSTIRPathtracingGraphicsPipeline::GlobalGIParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	}
}

void VulkanReSTIRPathtracingGraphicsPipeline::copy_global_uniform_data(VulkanReSTIRPathtracingGraphicsPipeline::GlobalGIParams& globalData, int current_frame_index)
{
	_engine->map_buffer(_engine->_allocator, _globalUniformsBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &globalData, sizeof(VulkanReSTIRPathtracingGraphicsPipeline::GlobalGIParams));
		});
}


void VulkanReSTIRPathtracingGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	{
		VkClearValue clear_value = { 0., 0., 0., 0. };
		cmd->clear_image(*_engine->get_engine_texture(ETextureResourceNames::ReSTIR_PT_OUTPUT), clear_value);
	}

	cmd->raytrace(&_rgenRegion, &_missRegion, &_hitRegion, &_callRegion, _imageExtent.width, _imageExtent.height, 1,
		[&](VkCommandBuffer cmd) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipeline(EPipelineType::ReSTIR_PT));
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_PT), 0,
				1, &_engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->set, 0, nullptr);

			EDescriptorResourceNames currentGlobalUniformsDesc = current_frame_index % 2 == 0
				? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0
				: EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_PT), 1,
				1, &_engine->get_engine_descriptor(currentGlobalUniformsDesc)->set, 0, nullptr);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_PT), 2,
				1, &_engine->get_engine_descriptor(EDescriptorResourceNames::GBuffer)->set, 0, nullptr);

			_rpDescrMan.bind_descriptor_set(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_PT), 3);
		});

}

void VulkanReSTIRPathtracingGraphicsPipeline::barrier_for_frag_read(VulkanCommandBuffer* cmd)
{
	vkutil::image_pipeline_barrier(cmd->get_cmd(), *_engine->get_engine_texture(ETextureResourceNames::ReSTIR_PT_OUTPUT), VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

const Texture& VulkanReSTIRPathtracingGraphicsPipeline::get_output() const
{
	return *_engine->get_engine_texture(ETextureResourceNames::ReSTIR_PT_OUTPUT);
}
