#include <graphic_pipeline/vk_gi_raytrace_graphics_pipeline.h>


#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <vk_camera.h>

void VulkanGIShadowsRaytracingGraphicsPipeline::init_textures(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		engine->_windowExtent.width,
		engine->_windowExtent.height,
		1
	};

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_DI_PREV_RESERVOIRS);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *engine->_resManager.get_engine_texture(ETextureResourceNames::ReSTIR_DI_PREV_RESERVOIRS);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_PREV_RESERVOIRS);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *engine->_resManager.get_engine_texture(ETextureResourceNames::ReSTIR_GI_PREV_RESERVOIRS);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INIT_RESERVOIRS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_DI_CURRENT_RESERVOIRS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_CURRENT_RESERVOIRS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_DI_SPACIAL_RESERVOIRS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SPACIAL_RESERVOIRS);
	}
	//ReSTIR GI INIT
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO_INIT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_INIT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_NORMAL_INIT);
	}
	//ReSTIR GI PREV
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO_PREV);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *engine->_resManager.get_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO_PREV);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_PREV);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *engine->_resManager.get_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_PREV);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_NORMAL_PREV);

		_engine->immediate_submit2([&](VulkanCommandBuffer& cmd) {

			Texture& prevTex = *engine->_resManager.get_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_NORMAL_PREV);
			VkClearValue clear_value = { 0., 0., 0., 0. };
			cmd.clear_image(prevTex, clear_value);

			vkutil::image_pipeline_barrier(cmd.get_cmd(), prevTex, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

			});
	}
	//ReSTIR GI CURRENT
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO_CURRENT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_CURRENT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_NORMAL_CURRENT);
	}
	//ReSTIR GI SPACIAL
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO_SPACIAL);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_SPACIAL);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_NORMAL_SPACIAL);
	}

	//REFLECTION
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::RAYTRACE_REFLECTION);
	}

}

void VulkanGIShadowsRaytracingGraphicsPipeline::init(VulkanEngine* engine)
{
	_engine = engine;

	{
		init_global_buffers();
		init_scene_descriptors();
	}

	_restir_DI_InitGP = std::make_unique<VulkanReSTIR_DI_InitPass>();
	_restir_DI_InitGP->init(engine);

	_restirInitGP = std::make_unique<VulkanReSTIRInitPass>();
	_restirInitGP->init(engine);

	_restirTemporalGP = std::make_unique<VulkanReSTIRTemporalPass>();
	_restirTemporalGP->init(engine);

	_restirSpacialGP = std::make_unique<VulkanReSTIRSpaceReusePass>();
	_restirSpacialGP->init(engine);

	_restir_PT_TemporalGP = std::make_unique<VulkanReSTIR_PT_TemporalPass>();
	_restir_PT_TemporalGP->init(engine);

	_restir_PT_SpacialGP = std::make_unique<VulkanReSTIR_PT_SpaceReusePass>();
	_restir_PT_SpacialGP->init(engine);

	if (_engine->get_mode() == ReSTIR_NRC)
	{
		_nrcTrainGP = std::make_unique<VulkanNRC_TrainingPass>();
		_nrcTrainGP->init(engine);

		_nrcOptimizeGP = std::make_unique<VulkanNRC_OptimizePass>();
		_nrcOptimizeGP->init(engine);

		_nrcInferenceGP = std::make_unique<VulkanNRC_InferencePass>();
		_nrcInferenceGP->init(engine);

		_accumulationGP = std::make_unique<VulkanSimpleAccumulationGraphicsPipeline>();
		_accumulationGP->init(engine, _nrcInferenceGP->get_output());
	}
	else
	{

		_restirUpdateShadeGP = std::make_unique<VulkanReSTIRUpdateReservoirPlusShadePass>();
		_restirUpdateShadeGP->init(engine);

		_accumulationGP = std::make_unique<VulkanSimpleAccumulationGraphicsPipeline>();
		_accumulationGP->init(engine, _restirUpdateShadeGP->get_output());
	}


	//_raytraceReflection = std::make_unique<VulkanRaytrace_ReflectionPass>();
	//_raytraceReflection->init(engine);

	//_denoiserPass = std::make_unique<VulkanRaytracerDenoiserPass>();
	//_denoiserPass->init(engine);
}


void VulkanGIShadowsRaytracingGraphicsPipeline::init_scene_descriptors()
{
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
				.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_COMPUTE_BIT)
				.build(_engine, currentDesciptor);
		}
	}

	if (_engine->get_mode() == ReSTIR_NRC)
	{
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			//set 1
			VkDescriptorBufferInfo nrcUniformsInfo;
			nrcUniformsInfo.buffer = _nrcUniformsBuffer[i]._buffer;
			nrcUniformsInfo.offset = 0;
			nrcUniformsInfo.range = VK_WHOLE_SIZE;

			EDescriptorResourceNames currentDesciptor = i == 0 ? EDescriptorResourceNames::NRC_GlobalUniformBuffer_Frame0 : EDescriptorResourceNames::NRC_GlobalUniformBuffer_Frame1;

			vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
				.bind_buffer(0, &nrcUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.build(_engine, currentDesciptor);
		}

		const uint32_t mlpDeviceBinding = 0;
		const uint32_t mlpParamsBinding = 1;
		const uint32_t mlpGradientsBinding = 2;
		const uint32_t mlpMoments1Binding = 3;
		const uint32_t mlpMoments2Binding = 4;

		VkDescriptorBufferInfo mlpDeviceInfo;
		mlpDeviceInfo.buffer = _engine->_resManager.nrc_cache->m_mlpDeviceBuffer._buffer;
		mlpDeviceInfo.offset = 0;
		mlpDeviceInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo mlpParamsInfo;
		mlpParamsInfo.buffer = _engine->_resManager.nrc_cache->m_mlpParamsBuffer32._buffer;
		mlpParamsInfo.offset = 0;
		mlpParamsInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo mlpGradientsInfo;
		mlpGradientsInfo.buffer = _engine->_resManager.nrc_cache->m_mlpGradientsBuffer._buffer;
		mlpGradientsInfo.offset = 0;
		mlpGradientsInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo mlpMoments1Info;
		mlpMoments1Info.buffer = _engine->_resManager.nrc_cache->m_mlpMoments1Buffer._buffer;
		mlpMoments1Info.offset = 0;
		mlpMoments1Info.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo mlpMoments2Info;
		mlpMoments2Info.buffer = _engine->_resManager.nrc_cache->m_mlpMoments2Buffer._buffer;
		mlpMoments2Info.offset = 0;
		mlpMoments2Info.range = VK_WHOLE_SIZE;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(mlpDeviceBinding, &mlpDeviceInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_buffer(mlpParamsBinding, &mlpParamsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_buffer(mlpGradientsBinding, &mlpGradientsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_buffer(mlpMoments1Binding, &mlpMoments1Info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_buffer(mlpMoments2Binding, &mlpMoments2Info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(_engine, EDescriptorResourceNames::NRC_MLP);
	}
}

void VulkanGIShadowsRaytracingGraphicsPipeline::init_global_buffers()
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_globalUniformsBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		if (_engine->get_mode() == ReSTIR_NRC)
		{
			_nrcUniformsBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(TrainingConstantBufferEntry), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		}
	}
}

void VulkanGIShadowsRaytracingGraphicsPipeline::copy_global_uniform_data(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams& globalData, int current_frame_index)
{
	globalData.widthScreen = _imageExtent.width;
	globalData.heightScreen = _imageExtent.height;
	_engine->map_buffer(_engine->_allocator, _globalUniformsBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &globalData, sizeof(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams));
		});
}



void VulkanGIShadowsRaytracingGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	{
		std::array<VkBufferMemoryBarrier, 2> barriers =
		{
			vkinit::buffer_barrier(_engine->_resManager.globalReservoirDIInitBuffer._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
			vkinit::buffer_barrier(_engine->_resManager.globalReservoirPTInitBuffer._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
	}

	uint32_t curTemporalIndx = (current_frame_index + 1) % 2;
	uint32_t prevTemporalIndx = current_frame_index % 2;
	_restir_DI_InitGP->draw(cmd, current_frame_index);
	_restirInitGP->draw(cmd, current_frame_index);
	{
		{
			std::array<VkBufferMemoryBarrier, 2> barriers =
			{
				vkinit::buffer_barrier(_engine->_resManager.globalReservoirDIInitBuffer._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
				vkinit::buffer_barrier(_engine->_resManager.globalReservoirDITemporalBuffer[prevTemporalIndx]._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT),
			};

			vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
		}

		{
			std::array<VkBufferMemoryBarrier, 1> barriers =
			{
				vkinit::buffer_barrier(_engine->_resManager.globalReservoirDITemporalBuffer[curTemporalIndx]._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
			};

			vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
		}
	}
	_restirTemporalGP->draw(cmd, current_frame_index);
	{
		std::array<VkBufferMemoryBarrier, 2> barriers =
		{
			vkinit::buffer_barrier(_engine->_resManager.globalReservoirDISpacialBuffer._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
			vkinit::buffer_barrier(_engine->_resManager.globalReservoirDITemporalBuffer[curTemporalIndx]._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
	}
	_restirSpacialGP->draw(cmd, current_frame_index);
	//ReSTIR PT
	{
		{
			std::array<VkBufferMemoryBarrier, 1> barriers =
			{
				vkinit::buffer_barrier(_engine->_resManager.globalReservoirPTInitBuffer._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
			};

			vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
		}
		{
			std::array<VkBufferMemoryBarrier, 1> barriers =
			{
				vkinit::buffer_barrier(_engine->_resManager.globalReservoirPTTemporalBuffer[prevTemporalIndx]._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT),
			};

			vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
		}

		{
			std::array<VkBufferMemoryBarrier, 1> barriers =
			{
				vkinit::buffer_barrier(_engine->_resManager.globalReservoirPTTemporalBuffer[curTemporalIndx]._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
			};

			vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
		}
	}
	_restir_PT_TemporalGP->draw(cmd, current_frame_index);
	{
		std::array<VkBufferMemoryBarrier, 2> barriers =
		{
			vkinit::buffer_barrier(_engine->_resManager.globalReservoirPTSpacialBuffer._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
			vkinit::buffer_barrier(_engine->_resManager.globalReservoirPTTemporalBuffer[curTemporalIndx]._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
	}
	_restir_PT_SpacialGP->draw(cmd, current_frame_index);
	{
		std::array<VkBufferMemoryBarrier, 1> barriers =
		{
			vkinit::buffer_barrier(_engine->_resManager.globalReservoirDISpacialBuffer._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
			//vkinit::buffer_barrier(_engine->_resManager.globalReservoirDITemporalBuffer[curTemporalIndx]._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
	}
	{
		std::array<VkBufferMemoryBarrier, 1> barriers =
		{
			vkinit::buffer_barrier(_engine->_resManager.globalReservoirPTSpacialBuffer._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT),
			//vkinit::buffer_barrier(_engine->_resManager.globalReservoirDITemporalBuffer[curTemporalIndx]._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);
	}
	//_restir_GI_TemporalGP->draw(cmd, current_frame_index);
	//_restir_GI_SpacialGP->draw(cmd, current_frame_index);

	//_raytraceReflection->draw(cmd, current_frame_index);

	if (_engine->get_mode() == ReSTIR_NRC)
	{
		static std::random_device rd;
		std::uniform_int_distribution<uint64_t> ldist;
		uint64_t seed = ldist(rd);
		for (int i = 0; i < BATCH_COUNT; ++i)
		{
			NeuralRadianceCache& nrc = *_engine->_resManager.nrc_cache.get();

			TrainingConstantBufferEntry trainingModelConstant = {
			.maxParamSize = nrc.m_totalParameterCount, .learningRate = nrc.m_learningRate, .currentStep = float(++nrc.m_currentOptimizationStep), .batchSize = nrc.m_batchSize, .seed = seed
			};

			std::ranges::copy(nrc.m_weightOffsets, trainingModelConstant.weightOffsets);
			std::ranges::copy(nrc.m_biasOffsets, trainingModelConstant.biasOffsets);

			_engine->map_buffer(_engine->_allocator, _nrcUniformsBuffer[current_frame_index]._allocation, [&](void*& data) {
				memcpy(data, &trainingModelConstant, sizeof(TrainingConstantBufferEntry));
				});

			_nrcTrainGP->draw(cmd, current_frame_index);

			_nrcOptimizeGP->draw(cmd, current_frame_index);
		}

		_nrcInferenceGP->barrier_for_compute_write(cmd);
		_nrcInferenceGP->draw(cmd, current_frame_index);

		_nrcInferenceGP->barrier_for_frag_read(cmd);
		_accumulationGP->draw(cmd, current_frame_index);
	}
	else
	{
		_restirUpdateShadeGP->barrier_for_compute_write(cmd);
		_restirUpdateShadeGP->draw(cmd, current_frame_index);

		_restirUpdateShadeGP->barrier_for_frag_read(cmd);
		_accumulationGP->draw(cmd, current_frame_index);
	}

	//_denoiserPass->draw(cmd, current_frame_index);
}

const Texture& VulkanGIShadowsRaytracingGraphicsPipeline::get_output() const
{
	return _accumulationGP->get_output();
}

void VulkanGIShadowsRaytracingGraphicsPipeline::reset_accumulation()
{
	//_accumulationGP->reset_accumulation();
}

void VulkanGIShadowsRaytracingGraphicsPipeline::try_reset_accumulation(PlayerCamera& camera)
{
	_accumulationGP->try_reset_accumulation(camera);
}

