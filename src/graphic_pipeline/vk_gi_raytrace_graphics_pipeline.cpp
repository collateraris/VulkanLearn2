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
#include <vk_render_graph.h>
#include <render_scene_resources.h>

void VulkanGIShadowsRaytracingGraphicsPipeline::init_textures(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		engine->_renderExtent.width,
		engine->_renderExtent.height,
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


	_reblurDenoiser = std::make_unique<ReblurDenoiserPass>();
	if (_engine->get_mode() == ReSTIR_NRC)
		_reblurDenoiser->init(engine, _nrcInferenceGP->get_diffuse_output(),
			_nrcInferenceGP->get_specular_output(), _nrcInferenceGP->get_bypass_output());
	else
		_reblurDenoiser->init(engine, _restirUpdateShadeGP->get_diffuse_output(),
			_restirUpdateShadeGP->get_specular_output(), _restirUpdateShadeGP->get_bypass_output());

	//_raytraceReflection = std::make_unique<VulkanRaytrace_ReflectionPass>();
	//_raytraceReflection->init(engine);

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



void VulkanGIShadowsRaytracingGraphicsPipeline::append_passes(rg::RenderGraph& graph, int frameSlot)
{
    using rhi::Access;
    using rhi::Layout;
    using rhi::Stage;
    using rg::Use;

    auto& device = _engine->_rhi;
    auto& resources = _engine->_resManager;
    const uint32_t currentTemporal = (_engine->_frameNumber + 1) % 2;
    const uint32_t previousTemporal = _engine->_frameNumber % 2;
    const bool historyValid = _historyValid;
    const bool useNrc = _engine->get_mode() == ERenderMode::ReSTIR_NRC;
    const Access readWrite = Access::ShaderRead | Access::ShaderWrite;

    auto buffer = [&](const char* name, const AllocatedBuffer& allocation, bool initialized = true) {
        return graph.import_resource(name, device.buffer(allocation), initialized);
    };
    auto image = [&](const char* name, const Texture& texture, bool initialized = false) {
        return graph.import_resource(name, device.image(texture), initialized);
    };
    auto storageUse = [](rg::ResourceHandle resource, Stage stage, Access access) {
        return Use{resource, {stage, access, Layout::General}};
    };

    const std::array<rg::ResourceHandle, 4> gbuffer = {
        image("ReSTIR.GBuffer.AlbedoMetalness", *resources.get_engine_texture(ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS)),
        image("ReSTIR.GBuffer.EmissionRoughness", *resources.get_engine_texture(ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS)),
        image("ReSTIR.GBuffer.Normal", *resources.get_engine_texture(ETextureResourceNames::PT_GBUFFER_NORMAL)),
        image("ReSTIR.GBuffer.PositionObject", *resources.get_engine_texture(ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID))
    };
    const auto giUniform = buffer("ReSTIR.FrameUniform", _globalUniformsBuffer[frameSlot]);
    const auto diInitial = buffer("ReSTIR.DI.Initial", resources.globalReservoirDIInitBuffer, false);
    const auto diCurrent = buffer("ReSTIR.DI.CurrentTemporal", resources.globalReservoirDITemporalBuffer[currentTemporal], false);
    const auto diPrevious = buffer("ReSTIR.DI.PreviousTemporal", resources.globalReservoirDITemporalBuffer[previousTemporal], historyValid);
    const auto diSpatial = buffer("ReSTIR.DI.Spatial", resources.globalReservoirDISpacialBuffer, false);
    const auto ptInitial = buffer("ReSTIR.PT.Initial", resources.globalReservoirPTInitBuffer, false);
    const auto ptCurrent = buffer("ReSTIR.PT.CurrentTemporal", resources.globalReservoirPTTemporalBuffer[currentTemporal], false);
    const auto ptPrevious = buffer("ReSTIR.PT.PreviousTemporal", resources.globalReservoirPTTemporalBuffer[previousTemporal], historyValid);
    const auto ptSpatial = buffer("ReSTIR.PT.Spatial", resources.globalReservoirPTSpacialBuffer, false);

    auto surfaceReads = [&](Stage stage, bool readsScene) {
        std::vector<Use> uses = readsScene ? import_scene_reads(*_engine, graph, stage) : std::vector<Use>{};
        uses.push_back(storageUse(giUniform, stage, Access::UniformRead));
        for (const auto texture : gbuffer)
            uses.push_back(storageUse(texture, stage, Access::ShaderRead));
        return uses;
    };

    {
        auto uses = import_scene_reads(*_engine, graph, Stage::RayTracing);
        uses.push_back(storageUse(giUniform, Stage::RayTracing, Access::UniformRead));
        uses.push_back(storageUse(diInitial, Stage::RayTracing, Access::ShaderWrite));
        uses.push_back(storageUse(buffer("ReSTIR.DI.ShaderTable", _restir_DI_InitGP->shader_binding_table_buffer()),
            Stage::RayTracing, Access::ShaderRead));
        for (const auto texture : gbuffer)
            uses.push_back(storageUse(texture, Stage::RayTracing, Access::ShaderWrite));
        graph.add_pass("ReSTIR.DI.Init", std::move(uses), [this, frameSlot](rhi::CommandList& cmd) {
            _restir_DI_InitGP->draw(cmd, frameSlot);
        });
    }
    {
        auto uses = surfaceReads(Stage::RayTracing, true);
        uses.push_back(storageUse(ptInitial, Stage::RayTracing, Access::ShaderWrite));
        uses.push_back(storageUse(buffer("ReSTIR.PT.InitShaderTable", _restirInitGP->shader_binding_table_buffer()),
            Stage::RayTracing, Access::ShaderRead));
        graph.add_pass("ReSTIR.PT.Init", std::move(uses), [this, frameSlot](rhi::CommandList& cmd) {
            _restirInitGP->draw(cmd, frameSlot);
        });
    }
    {
        auto uses = surfaceReads(Stage::Compute, true);
        uses.push_back(storageUse(diInitial, Stage::Compute, Access::ShaderRead));
        uses.push_back(storageUse(diCurrent, Stage::Compute, Access::ShaderWrite));
        if (historyValid)
            uses.push_back(storageUse(diPrevious, Stage::Compute, Access::ShaderRead));
        graph.add_pass("ReSTIR.DI.Temporal", std::move(uses), [this, frameSlot](rhi::CommandList& cmd) {
            _restirTemporalGP->draw(cmd, frameSlot);
        });
    }
    {
        auto uses = surfaceReads(Stage::Compute, true);
        uses.push_back(storageUse(diCurrent, Stage::Compute, Access::ShaderRead));
        uses.push_back(storageUse(diSpatial, Stage::Compute, Access::ShaderWrite));
        graph.add_pass("ReSTIR.DI.Spatial", std::move(uses), [this, frameSlot](rhi::CommandList& cmd) {
            _restirSpacialGP->draw(cmd, frameSlot);
        });
    }
    // This shader only copies the initial PT reservoir into the current slot.
    graph.add_pass("ReSTIR.PT.PrepareTemporal", {
        storageUse(giUniform, Stage::Compute, Access::UniformRead),
        storageUse(ptInitial, Stage::Compute, Access::ShaderRead),
        storageUse(ptCurrent, Stage::Compute, Access::ShaderWrite)
    }, [this, frameSlot](rhi::CommandList& cmd) {
        _restir_PT_TemporalGP->draw(cmd, frameSlot);
    });
    {
        auto uses = surfaceReads(Stage::RayTracing, true);
        uses.push_back(storageUse(ptInitial, Stage::RayTracing, Access::ShaderRead));
        uses.push_back(storageUse(ptCurrent, Stage::RayTracing, readWrite));
        uses.push_back(storageUse(ptSpatial, Stage::RayTracing, Access::ShaderWrite));
        uses.push_back(storageUse(buffer("ReSTIR.PT.ReuseShaderTable", _restir_PT_SpacialGP->shader_binding_table_buffer()),
            Stage::RayTracing, Access::ShaderRead));
        if (historyValid)
            uses.push_back(storageUse(ptPrevious, Stage::RayTracing, Access::ShaderRead));
        graph.add_pass("ReSTIR.PT.ReplayAndSpatial", std::move(uses), [this, frameSlot](rhi::CommandList& cmd) {
            _restir_PT_SpacialGP->draw(cmd, frameSlot);
        });
    }

    if (useNrc)
    {
        NeuralRadianceCache& nrc = *resources.nrc_cache;
        const auto nrcUniform = buffer("NRC.FrameUniform", _nrcUniformsBuffer[frameSlot]);
        const auto weights = buffer("NRC.WeightsFP16", nrc.m_mlpDeviceBuffer);
        const auto masterWeights = buffer("NRC.WeightsFP32", nrc.m_mlpParamsBuffer32);
        const auto gradients = buffer("NRC.Gradients", nrc.m_mlpGradientsBuffer);
        const auto moments1 = buffer("NRC.Moments1", nrc.m_mlpMoments1Buffer);
        const auto moments2 = buffer("NRC.Moments2", nrc.m_mlpMoments2Buffer);
        const auto gradientIndices = buffer("NRC.GradientIndices", nrc.m_gradientIndexMapBuffer);

        if (_resetNrcTraining)
        {
            const std::array<rhi::Resource, 3> resetBuffers = {
                device.buffer(nrc.m_mlpGradientsBuffer),
                device.buffer(nrc.m_mlpMoments1Buffer),
                device.buffer(nrc.m_mlpMoments2Buffer)
            };
            graph.add_pass("NRC.ResetOptimizer", {
                storageUse(gradients, Stage::Transfer, Access::TransferWrite),
                storageUse(moments1, Stage::Transfer, Access::TransferWrite),
                storageUse(moments2, Stage::Transfer, Access::TransferWrite)
            }, [resetBuffers](rhi::CommandList& cmd) {
                for (const auto resource : resetBuffers)
                    cmd.fill_buffer(resource, 0);
            });
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
        _engine->write_buffer(_engine->_allocator, _nrcUniformsBuffer[frameSlot]._allocation,
                             &trainingModelConstant, sizeof(trainingModelConstant));

        {
            auto uses = surfaceReads(Stage::Compute, false);
            uses.push_back(storageUse(nrcUniform, Stage::Compute, Access::UniformRead));
            uses.push_back(storageUse(ptSpatial, Stage::Compute, Access::ShaderRead));
            uses.push_back(storageUse(weights, Stage::Compute, Access::ShaderRead));
            uses.push_back(storageUse(gradients, Stage::Compute, readWrite));
            graph.add_pass("NRC.Train", std::move(uses), [this, frameSlot](rhi::CommandList& cmd) {
                _nrcTrainGP->draw(cmd, frameSlot);
            });
        }
        graph.add_pass("NRC.Optimize", {
            storageUse(nrcUniform, Stage::Compute, Access::UniformRead),
            storageUse(weights, Stage::Compute, readWrite),
            storageUse(masterWeights, Stage::Compute, readWrite),
            storageUse(gradients, Stage::Compute, readWrite),
            storageUse(moments1, Stage::Compute, readWrite),
            storageUse(moments2, Stage::Compute, readWrite),
            storageUse(gradientIndices, Stage::Compute, Access::ShaderRead)
        }, [this, frameSlot](rhi::CommandList& cmd) {
            _nrcOptimizeGP->draw(cmd, frameSlot);
        });
        {
            auto uses = surfaceReads(Stage::Compute, true);
            uses.push_back(storageUse(nrcUniform, Stage::Compute, Access::UniformRead));
            uses.push_back(storageUse(diSpatial, Stage::Compute, Access::ShaderRead));
            uses.push_back(storageUse(ptSpatial, Stage::Compute, Access::ShaderRead));
            uses.push_back(storageUse(weights, Stage::Compute, Access::ShaderRead));
            uses.push_back(storageUse(image("NRC.Output", _nrcInferenceGP->get_output()),
                Stage::Compute, Access::ShaderWrite));
            uses.push_back(storageUse(image("NRC.DiffuseSignal", _nrcInferenceGP->get_diffuse_output()),
                Stage::Compute, Access::ShaderWrite));
            uses.push_back(storageUse(image("NRC.SpecularSignal", _nrcInferenceGP->get_specular_output()),
                Stage::Compute, Access::ShaderWrite));
            uses.push_back(storageUse(image("NRC.BypassSignal", _nrcInferenceGP->get_bypass_output()),
                Stage::Compute, Access::ShaderWrite));
            graph.add_pass("NRC.Inference", std::move(uses), [this, frameSlot](rhi::CommandList& cmd) {
                _nrcInferenceGP->draw(cmd, frameSlot);
            });
        }
    }
    else
    {
        auto uses = surfaceReads(Stage::Compute, true);
        uses.push_back(storageUse(diSpatial, Stage::Compute, Access::ShaderRead));
        uses.push_back(storageUse(ptSpatial, Stage::Compute, Access::ShaderRead));
        uses.push_back(storageUse(image("ReSTIR.ShadedOutput", _restirUpdateShadeGP->get_output()),
            Stage::Compute, Access::ShaderWrite));
        uses.push_back(storageUse(image("ReSTIR.DiffuseSignal", _restirUpdateShadeGP->get_diffuse_output()),
            Stage::Compute, Access::ShaderWrite));
        uses.push_back(storageUse(image("ReSTIR.SpecularSignal", _restirUpdateShadeGP->get_specular_output()),
            Stage::Compute, Access::ShaderWrite));
        uses.push_back(storageUse(image("ReSTIR.BypassSignal", _restirUpdateShadeGP->get_bypass_output()),
            Stage::Compute, Access::ShaderWrite));
        graph.add_pass("ReSTIR.Shade", std::move(uses), [this, frameSlot](rhi::CommandList& cmd) {
            _restirUpdateShadeGP->draw(cmd, frameSlot);
        });
    }

    const auto accumulationFirst = graph.pass_count();
    _accumulationGP->append_passes(graph, frameSlot);
    const auto accumulationEnd = graph.pass_count();
    const auto queryPool = device.query_pool(_engine->get_current_frame().queryPool);
    // Timestamp boundaries are explicit control dependencies, so they remain
    // correct if independent resource passes are scheduled differently later.
    const auto denoiserBegin = graph.add_pass("Denoiser.TimestampBegin", {}, [queryPool](rhi::CommandList& cmd) {
        cmd.timestamp(queryPool, 2, rhi::Stage::Bottom);
    });
    for (auto pass = accumulationFirst; pass < accumulationEnd; ++pass)
        graph.depends_on(denoiserBegin, static_cast<rg::PassHandle>(pass));
    const auto denoiserFirst = graph.pass_count();
    _reblurDenoiser->append_passes(graph, static_cast<uint32_t>(frameSlot));
    const auto denoiserLast = graph.pass_count();
    const auto denoiserEnd = graph.add_pass("Denoiser.TimestampEnd", {}, [queryPool](rhi::CommandList& cmd) {
        cmd.timestamp(queryPool, 3, rhi::Stage::Bottom);
    });
    graph.depends_on(denoiserEnd, denoiserBegin);
    for (auto pass = denoiserFirst; pass < denoiserLast; ++pass)
    {
        graph.depends_on(static_cast<rg::PassHandle>(pass), denoiserBegin);
        graph.depends_on(denoiserEnd, static_cast<rg::PassHandle>(pass));
    }
    const auto commit = graph.add_pass("ReSTIR.CommitHistory", {}, [this](rhi::CommandList&) {
        _historyValid = true;
    });
    graph.depends_on(commit, denoiserEnd);
}
const Texture& VulkanGIShadowsRaytracingGraphicsPipeline::get_output() const
{
	return _accumulationGP->get_output();
}

const Texture& VulkanGIShadowsRaytracingGraphicsPipeline::get_denoised_output() const
{
	return _reblurDenoiser->get_output();
}

const Texture& VulkanGIShadowsRaytracingGraphicsPipeline::get_display_output() const
{
	return _engine->_denoiserEnabled ? get_denoised_output() : get_output();
}

void VulkanGIShadowsRaytracingGraphicsPipeline::reset_accumulation()
{
	_accumulationGP->reset_accumulation();
	_reblurDenoiser->reset_history();
	_historyValid = false;
	_resetNrcTraining = true;
}

void VulkanGIShadowsRaytracingGraphicsPipeline::try_reset_accumulation(PlayerCamera& camera)
{
	_accumulationGP->try_reset_accumulation(camera);
}

