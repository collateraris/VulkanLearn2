#include <graphic_pipeline/vk_gi_raytrace_graphics_pipeline.h>

#if GI_RAYTRACER_ON

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
			.create_engine_texture(ETextureResourceNames::ReSTIR_INDIRECT_LO);
	}


}

void VulkanGIShadowsRaytracingGraphicsPipeline::init(VulkanEngine* engine)
{
	_engine = engine;

	{
		init_global_buffers();
		init_scene_descriptors();
	}

	_restirInitGP = std::make_unique<VulkanReSTIRInitPass>();
	_restirInitGP->init(engine);

	_restirTemporalGP = std::make_unique<VulkanReSTIRTemporalPass>();
	_restirTemporalGP->init(engine);

	_restirSpacialGP = std::make_unique<VulkanReSTIRSpaceReusePass>();
	_restirSpacialGP->init(engine);

	_restirUpdateShadeGP = std::make_unique<VulkanReSTIRUpdateReservoirPlusShadePass>();
	_restirUpdateShadeGP->init(engine);

	_accumulationGP = std::make_unique<VulkanSimpleAccumulationGraphicsPipeline>();
	_accumulationGP->init(engine, _restirUpdateShadeGP->get_output());

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

			VkDescriptorImageInfo vbufferImageBufferInfo;
			vbufferImageBufferInfo.sampler = VK_NULL_HANDLE;
			vbufferImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::VBUFFER)->imageView;
			vbufferImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			EDescriptorResourceNames currentDesciptor = i == 0 ? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0 : EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;

			vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
				.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
				.bind_image(1, &vbufferImageBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
				.build(_engine, currentDesciptor);
		}
	}
}

void VulkanGIShadowsRaytracingGraphicsPipeline::init_global_buffers()
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_globalUniformsBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	}
}

void VulkanGIShadowsRaytracingGraphicsPipeline::copy_global_uniform_data(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams& globalData, int current_frame_index)
{
	_engine->map_buffer(_engine->_allocator, _globalUniformsBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &globalData, sizeof(VulkanGIShadowsRaytracingGraphicsPipeline::GlobalGIParams));
		});
}



void VulkanGIShadowsRaytracingGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	vkutil::image_pipeline_barrier(cmd->get_cmd(), *_engine->get_engine_texture(ETextureResourceNames::VBUFFER), VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	_restirInitGP->draw(cmd, current_frame_index);
	_restirTemporalGP->draw(cmd, current_frame_index);
	_restirSpacialGP->draw(cmd, current_frame_index);
	_restirUpdateShadeGP->draw(cmd, current_frame_index);

	_restirUpdateShadeGP->barrier_for_frag_read(cmd);
	_accumulationGP->draw(cmd, current_frame_index);

	//_denoiserPass->draw(cmd, current_frame_index);
}

const Texture& VulkanGIShadowsRaytracingGraphicsPipeline::get_output() const
{
	return _accumulationGP->get_output();
}

void VulkanGIShadowsRaytracingGraphicsPipeline::reset_accumulation()
{
	_accumulationGP->reset_accumulation();
}

void VulkanGIShadowsRaytracingGraphicsPipeline::try_reset_accumulation(PlayerCamera& camera)
{
	_accumulationGP->try_reset_accumulation(camera);
}

#endif
