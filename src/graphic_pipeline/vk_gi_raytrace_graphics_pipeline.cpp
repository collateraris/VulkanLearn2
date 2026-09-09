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
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INIT_RESERVOIRS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_DI_CURRENT_RESERVOIRS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_CURRENT_RESERVOIRS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_DI_SPACIAL_RESERVOIRS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO_INIT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_INIT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO_CURRENT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_CURRENT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO_SPACIAL);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_giSamplesColorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_SPACIAL);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_giSamplesColorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
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


	_spatialDenoiser = std::make_unique<VulkanSpatialDenoiserPass>();
	_spatialDenoiser->init(engine, _accumulationGP->get_output());

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

		{
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

			VkDescriptorBufferInfo gradientIndexMapInfo{};
			gradientIndexMapInfo.buffer = _engine->_resManager.nrc_cache->m_gradientIndexMapBuffer._buffer;
			gradientIndexMapInfo.range = VK_WHOLE_SIZE;

			vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
				.bind_buffer(mlpDeviceBinding, &mlpDeviceInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bind_buffer(mlpParamsBinding, &mlpParamsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bind_buffer(mlpGradientsBinding, &mlpGradientsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bind_buffer(mlpMoments1Binding, &mlpMoments1Info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bind_buffer(mlpMoments2Binding, &mlpMoments2Info, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bind_buffer(5, &gradientIndexMapInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.build(_engine, EDescriptorResourceNames::NRC_MLP_Optimize);
		}

		{
			const uint32_t mlpDeviceBinding = 0;
			const uint32_t mlpGradientsBinding = 1;

			VkDescriptorBufferInfo mlpDeviceInfo;
			mlpDeviceInfo.buffer = _engine->_resManager.nrc_cache->m_mlpDeviceBuffer._buffer;
			mlpDeviceInfo.offset = 0;
			mlpDeviceInfo.range = VK_WHOLE_SIZE;


			VkDescriptorBufferInfo mlpGradientsInfo;
			mlpGradientsInfo.buffer = _engine->_resManager.nrc_cache->m_mlpGradientsBuffer._buffer;
			mlpGradientsInfo.offset = 0;
			mlpGradientsInfo.range = VK_WHOLE_SIZE;


			vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
				.bind_buffer(mlpDeviceBinding, &mlpDeviceInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.bind_buffer(mlpGradientsBinding, &mlpGradientsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.build(_engine, EDescriptorResourceNames::NRC_MLP_Train_Inference);
		}
	}
}

void VulkanGIShadowsRaytracingGraphicsPipeline::init_global_buffers()
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_globalUniformsBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		_engine->_mainDeletionQueue.push_function([engine = _engine, buffer = _globalUniformsBuffer[i]]() mutable {
			engine->destroy_buffer(engine->_allocator, buffer);
		});
		if (_engine->get_mode() == ReSTIR_NRC)
		{
			_nrcUniformsBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(TrainingConstantBufferEntry), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
			_engine->_mainDeletionQueue.push_function([engine = _engine, buffer = _nrcUniformsBuffer[i]]() mutable {
				engine->destroy_buffer(engine->_allocator, buffer);
			});
		}
	}
}

void VulkanGIShadowsRaytracingGraphicsPipeline::copy_global_uniform_data(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams& globalData, int current_frame_index)
{
	globalData.widthScreen = _imageExtent.width;
	globalData.heightScreen = _imageExtent.height;
	const auto view = _engine->_camera.get_view_matrix();
	const auto projection = _engine->_camera.get_projection_matrix(false);
	if (_historyView != view || _historyProjection != projection)
	{
		_historyValid = false;
		_accumulationGP->reset_accumulation();
	}
	_historyView = view;
	_historyProjection = projection;
	globalData.historyValid = _historyValid ? 1u : 0u;
	globalData.environmentTextureIndex = _engine->_resManager.environmentTextureIndex;
	globalData.environmentIntensity = _engine->_resManager.environmentIntensity;
	globalData.indirectSunScale = _engine->_resManager.indirectSunScale;
	_engine->map_buffer(_engine->_allocator, _globalUniformsBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &globalData, sizeof(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams));
		});
}



void VulkanGIShadowsRaytracingGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
    const VkPipelineStageFlags traceStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    const VkPipelineStageFlags computeStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    const VkPipelineStageFlags shaderStages = traceStage | computeStage;
    const VkAccessFlags readWrite = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    auto memoryBarrier = [&](VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                             VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        vkCmdPipelineBarrier(cmd->get_cmd(), srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    };

    // Reservoirs and G-buffer images are shared across frames on this queue.
    // Include both reads and writes before overwriting an earlier frame's data.
    memoryBarrier(shaderStages, shaderStages, readWrite, readWrite);
    _restir_DI_InitGP->draw(cmd, current_frame_index);
    memoryBarrier(traceStage, shaderStages, VK_ACCESS_SHADER_WRITE_BIT, readWrite);
    _restirInitGP->draw(cmd, current_frame_index);
    memoryBarrier(traceStage, computeStage, VK_ACCESS_SHADER_WRITE_BIT, readWrite);
    _restirTemporalGP->draw(cmd, current_frame_index);
    memoryBarrier(computeStage, computeStage, VK_ACCESS_SHADER_WRITE_BIT, readWrite);
    _restirSpacialGP->draw(cmd, current_frame_index);
    memoryBarrier(computeStage, computeStage, VK_ACCESS_SHADER_WRITE_BIT, readWrite);
    _restir_PT_TemporalGP->draw(cmd, current_frame_index);
    memoryBarrier(computeStage, traceStage, VK_ACCESS_SHADER_WRITE_BIT, readWrite);
    _restir_PT_SpacialGP->draw(cmd, current_frame_index);
    memoryBarrier(shaderStages, computeStage, VK_ACCESS_SHADER_WRITE_BIT, readWrite);

    if (_engine->get_mode() == ERenderMode::ReSTIR_NRC)
    {
        NeuralRadianceCache& nrc = *_engine->_resManager.nrc_cache;
        if (_resetNrcTraining)
        {
            memoryBarrier(computeStage, VK_PIPELINE_STAGE_TRANSFER_BIT, readWrite, VK_ACCESS_TRANSFER_WRITE_BIT);
            vkCmdFillBuffer(cmd->get_cmd(), nrc.m_mlpGradientsBuffer._buffer, 0, VK_WHOLE_SIZE, 0);
            vkCmdFillBuffer(cmd->get_cmd(), nrc.m_mlpMoments1Buffer._buffer, 0, VK_WHOLE_SIZE, 0);
            vkCmdFillBuffer(cmd->get_cmd(), nrc.m_mlpMoments2Buffer._buffer, 0, VK_WHOLE_SIZE, 0);
            memoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, computeStage, VK_ACCESS_TRANSFER_WRITE_BIT, readWrite);
            nrc.m_currentOptimizationStep = 0;
            _resetNrcTraining = false;
        }

        // One minibatch per frame, with one immutable uniform buffer per frame in flight.
        TrainingConstantBufferEntry trainingModelConstant{};
        trainingModelConstant.maxParamSize = nrc.m_totalParameterCount;
        trainingModelConstant.learningRate = nrc.m_learningRate;
        trainingModelConstant.currentStep = float(++nrc.m_currentOptimizationStep);
        trainingModelConstant.batchSize = nrc.m_batchSize;
        trainingModelConstant.seed = uint64_t(_engine->_frameNumber) + 1;
        std::ranges::copy(nrc.m_weightOffsets, trainingModelConstant.weightOffsets);
        std::ranges::copy(nrc.m_biasOffsets, trainingModelConstant.biasOffsets);
        std::ranges::copy(nrc.m_gradientWeightOffsets, trainingModelConstant.gradientWeightOffsets);
        std::ranges::copy(nrc.m_gradientBiasOffsets, trainingModelConstant.gradientBiasOffsets);
        _engine->write_buffer(_engine->_allocator, _nrcUniformsBuffer[current_frame_index]._allocation,
                             &trainingModelConstant, sizeof(trainingModelConstant));

        // Inference from the previous frame must finish reading the weights before Adam writes them.
        memoryBarrier(computeStage, computeStage, readWrite, readWrite);
        _nrcTrainGP->draw(cmd, current_frame_index);
        memoryBarrier(computeStage, computeStage, readWrite, readWrite);
        _nrcOptimizeGP->draw(cmd, current_frame_index);
        memoryBarrier(computeStage, computeStage, VK_ACCESS_SHADER_WRITE_BIT, readWrite);
        _nrcInferenceGP->barrier_for_compute_write(cmd);
        _nrcInferenceGP->draw(cmd, current_frame_index);
        _nrcInferenceGP->barrier_for_frag_read(cmd);
    }
    else
    {
        _restirUpdateShadeGP->barrier_for_compute_write(cmd);
        _restirUpdateShadeGP->draw(cmd, current_frame_index);
        _restirUpdateShadeGP->barrier_for_frag_read(cmd);
    }

    _accumulationGP->draw(cmd, current_frame_index);
    vkCmdWriteTimestamp(cmd->get_cmd(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        _engine->get_current_frame().queryPool, 2);
    _spatialDenoiser->draw(cmd, current_frame_index);
    vkCmdWriteTimestamp(cmd->get_cmd(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        _engine->get_current_frame().queryPool, 3);
    _historyValid = true;
}
const Texture& VulkanGIShadowsRaytracingGraphicsPipeline::get_output() const
{
	return _accumulationGP->get_output();
}

const Texture& VulkanGIShadowsRaytracingGraphicsPipeline::get_denoised_output() const
{
	return _spatialDenoiser->get_output();
}

const Texture& VulkanGIShadowsRaytracingGraphicsPipeline::get_display_output() const
{
	return _engine->_denoiserEnabled ? get_denoised_output() : get_output();
}

void VulkanGIShadowsRaytracingGraphicsPipeline::reset_accumulation()
{
	_accumulationGP->reset_accumulation();
	_spatialDenoiser->reset_history();
	_historyValid = false;
	_resetNrcTraining = true;
}

void VulkanGIShadowsRaytracingGraphicsPipeline::try_reset_accumulation(PlayerCamera& camera)
{
	_accumulationGP->try_reset_accumulation(camera);
}

