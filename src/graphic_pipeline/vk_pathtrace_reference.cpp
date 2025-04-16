#include <graphic_pipeline/vk_pathtrace_reference.h>
 

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <vk_utils.h>

void VulkanPTRef::init(VulkanEngine* engine)
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
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::PT_Reference,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				uint32_t rayGenIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("raytrace_reference.rgen.slang.spv")), VK_SHADER_STAGE_RAYGEN_BIT_NV);
				uint32_t rayIndirectMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("indirect_raytrace_gbuffer.rmiss.slang.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayShadowMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("shadow.rmiss.slang.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayIndirectClosestHitIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("indirect_raytrace_gbuffer.rchit.slang.spv")), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);

				defaultEffect.reflect_layout(engine->_device, nullptr, 0);

				RTPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->setLayout,
																 _globalDescSetLayout,
																 _rpDescrMan.get_layout()};
				mesh_pipeline_layout_info.setLayoutCount = setLayout.size();
				mesh_pipeline_layout_info.pSetLayouts = setLayout.data();

				vkCreatePipelineLayout(_engine->_device, &mesh_pipeline_layout_info, nullptr, &pipelineBuilder._pipelineLayout);

				pipelineBuilder._rayPipelineInfo.maxPipelineRayRecursionDepth = 1;  // Ray depth

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

	_rtSBTBuffer = VulkanRaytracerBuilder::create_SBTBuffer(engine, 2, 1, EPipelineType::PT_Reference,
		_rgenRegion,
		_missRegion,
		_hitRegion,
		_callRegion);
}

void VulkanPTRef::init_tex()
{
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::PT_REFERENCE_OUTPUT);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::PT_REFERENCE_ACCUMULATE);
	}
}

Texture& VulkanPTRef::get_tex(ETextureResourceNames name) const
{
	return *_engine->get_engine_texture(name);
}

void VulkanPTRef::reset_accumulation()
{
	bResetAccumulation = true;
}

void VulkanPTRef::init_description_set_global_buffer()
{
	_rpDescrMan = vkutil::DescriptorManagerBuilder::begin(_engine, _engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_image(0, ETextureResourceNames::PT_REFERENCE_OUTPUT, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(1, ETextureResourceNames::PT_REFERENCE_ACCUMULATE, EResOp::WRITE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.create_desciptor_manager();

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{

		_globalUniformsBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanPTRef::GlobalGIParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		VkDescriptorBufferInfo globalUniformsInfo;
		globalUniformsInfo.buffer = _globalUniformsBuffer[i]._buffer;
		globalUniformsInfo.offset = 0;
		globalUniformsInfo.range = VK_WHOLE_SIZE;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.build(_globalDescSet[i], _globalDescSetLayout);
	}
}

void VulkanPTRef::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	{
		VkClearValue clear_value = { 0., 0., 0., 0. };
		cmd->clear_image(get_tex(ETextureResourceNames::PT_REFERENCE_OUTPUT), clear_value);
	}

	if (bResetAccumulation)
	{
		bResetAccumulation = false;
		VkClearValue clear_value = { 0., 0., 0., 0. };
		cmd->clear_image(get_tex(ETextureResourceNames::PT_REFERENCE_ACCUMULATE), clear_value);
	}

	cmd->raytrace(&_rgenRegion, &_missRegion, &_hitRegion, &_callRegion, _imageExtent.width, _imageExtent.height, 1,
		[&](VkCommandBuffer cmd) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipeline(EPipelineType::PT_Reference));
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::PT_Reference), 0,
				1, &_engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->set, 0, nullptr);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::PT_Reference), 1, 1, &_globalDescSet[current_frame_index], 0, nullptr);

			_rpDescrMan.bind_descriptor_set(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::PT_Reference), 2);
		});
}

void VulkanPTRef::copy_global_uniform_data(VulkanPTRef::GlobalGIParams& giData, int current_frame_index)
{
	_engine->map_buffer(_engine->_allocator, _globalUniformsBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &giData, sizeof(VulkanPTRef::GlobalGIParams));
		});
}

void VulkanPTRef::barrier_for_reading(VulkanCommandBuffer* cmd)
{
	std::array<VkImageMemoryBarrier, 1> barriers =
	{
		vkinit::image_barrier(get_tex(ETextureResourceNames::PT_REFERENCE_OUTPUT).image._image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
	};

	vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, barriers.size(), barriers.data());
}

void VulkanPTRef::barrier_for_writing(VulkanCommandBuffer* cmd)
{
	std::array<VkImageMemoryBarrier, 1> barriers =
	{
		vkinit::image_barrier(get_tex(ETextureResourceNames::PT_REFERENCE_OUTPUT).image._image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT),
	};

	vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, 0, 0, 0, barriers.size(), barriers.data());
}


