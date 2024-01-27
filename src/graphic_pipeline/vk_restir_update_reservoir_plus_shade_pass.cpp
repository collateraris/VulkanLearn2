#include <graphic_pipeline/vk_restir_update_reservoir_plus_shade_pass.h>

#if GI_RAYTRACER_ON && GBUFFER_ON

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>

void VulkanReSTIRUpdateReservoirPlusShadePass::init(VulkanEngine* engine, VkAccelerationStructureKHR  tlas,
	std::array<AllocatedBuffer, 2>& globalUniformsBuffer, AllocatedBuffer& objectBuffer, 
	const Texture& indirectInput, const Texture& reservoirPrev, const Texture& reservoirSpatial)
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
		init_description_set();
		init_bindless(_engine->_resManager.meshList, _engine->_resManager.textureList, tlas);
		init_description_set_global_buffer(globalUniformsBuffer, objectBuffer,
			indirectInput, reservoirPrev, reservoirSpatial);
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
				std::vector<VkDescriptorSetLayout> setLayout = { _bindlessSetLayout, _globalUniformsDescSetLayout, _rtDescSetLayout, _gBuffDescSetLayout };
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

void VulkanReSTIRUpdateReservoirPlusShadePass::init_description_set_global_buffer(std::array<AllocatedBuffer, 2>& globalUniformsBuffer, AllocatedBuffer& objectBuffer, const Texture& indirectInput, const Texture& reservoirPrev, const Texture& reservoirSpatial)
{
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSampler sampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &sampler);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VkDescriptorImageInfo indirectInputImageBufferInfo;
		indirectInputImageBufferInfo.sampler = sampler;
		indirectInputImageBufferInfo.imageView = indirectInput.imageView;
		indirectInputImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo reservoirSpatialImageBufferInfo;
		reservoirSpatialImageBufferInfo.sampler = sampler;
		reservoirSpatialImageBufferInfo.imageView = reservoirSpatial.imageView;
		reservoirSpatialImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo reservoirPrevImageBufferInfo;
		reservoirPrevImageBufferInfo.sampler = sampler;
		reservoirPrevImageBufferInfo.imageView = reservoirPrev.imageView;
		reservoirPrevImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkDescriptorImageInfo outputBufferInfo;
		outputBufferInfo.sampler = sampler;
		outputBufferInfo.imageView = _outputTex.imageView;
		outputBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_image(0, &indirectInputImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(1, &reservoirSpatialImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(2, &reservoirPrevImageBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(3, &outputBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.build(_rtDescSet[i], _rtDescSetLayout);

		//set 1

		VkDescriptorBufferInfo globalUniformsInfo;
		globalUniformsInfo.buffer = globalUniformsBuffer[i]._buffer;
		globalUniformsInfo.offset = 0;
		globalUniformsInfo.range = globalUniformsBuffer[i]._size;

		VkDescriptorBufferInfo lightsInfo;
		lightsInfo.buffer = _engine->_lightManager.get_light_buffer(i)._buffer;
		lightsInfo.offset = 0;
		lightsInfo.range = _engine->_lightManager.get_light_buffer(i)._size;

		VkDescriptorBufferInfo objectBufferInfo;
		objectBufferInfo.buffer = objectBuffer._buffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = objectBuffer._size;


		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
			.bind_buffer(1, &lightsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
			.bind_buffer(2, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
			.build(_globalUniformsDescSet[i], _globalUniformsDescSetLayout);
	}
}

void VulkanReSTIRUpdateReservoirPlusShadePass::init_description_set()
{
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSampler sampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &sampler);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VkDescriptorImageInfo wposImageBufferInfo;
		wposImageBufferInfo.sampler = sampler;
		wposImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_WPOS)->imageView;
		wposImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo normalImageBufferInfo;
		normalImageBufferInfo.sampler = sampler;
		normalImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_NORM)->imageView;
		normalImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo uvImageBufferInfo;
		uvImageBufferInfo.sampler = sampler;
		uvImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_UV)->imageView;
		uvImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo objIDImageBufferInfo;
		objIDImageBufferInfo.sampler = sampler;
		objIDImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_OBJ_ID)->imageView;
		objIDImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo irradMapImageBufferInfo;
		irradMapImageBufferInfo.sampler = sampler;
		irradMapImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::IBL_IRRADIANCE)->imageView;
		irradMapImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo prefilteredMapImageBufferInfo;
		prefilteredMapImageBufferInfo.sampler = sampler;
		prefilteredMapImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::IBL_PREFILTEREDENV)->imageView;
		prefilteredMapImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo brdflutMapImageBufferInfo;
		brdflutMapImageBufferInfo.sampler = sampler;
		brdflutMapImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::IBL_BRDFLUT)->imageView;
		brdflutMapImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_image(0, &wposImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(1, &normalImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(2, &uvImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(3, &objIDImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(4, &irradMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(5, &prefilteredMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(6, &brdflutMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.build(_gBuffDescSet[i], _gBuffDescSetLayout);
	}
}

void VulkanReSTIRUpdateReservoirPlusShadePass::init_bindless(const std::vector<std::unique_ptr<Mesh>>& meshList, const std::vector<Texture*>& textureList, VkAccelerationStructureKHR  tlas)
{
	const uint32_t verticesBinding = 0;
	const uint32_t textureBinding = 1;
	const uint32_t tlasBinding = 2;

	//BIND MESHDATA
	std::vector<VkDescriptorBufferInfo> vertexBufferInfoList{};
	vertexBufferInfoList.resize(meshList.size());


	for (uint32_t meshArrayIndex = 0; meshArrayIndex < meshList.size(); meshArrayIndex++)
	{
		Mesh* mesh = meshList[meshArrayIndex].get();

		VkDescriptorBufferInfo& vertexBufferInfo = vertexBufferInfoList[meshArrayIndex];
		vertexBufferInfo.buffer = mesh->_vertexBufferRT._buffer;
		vertexBufferInfo.offset = 0;
		vertexBufferInfo.range = mesh->_vertexBufferRT._size;
	}

	//BIND SAMPLERS
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR);

	VkSampler blockySampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &blockySampler);

	std::vector<VkDescriptorImageInfo> imageInfoList{};
	imageInfoList.resize(textureList.size());

	for (uint32_t textureArrayIndex = 0; textureArrayIndex < textureList.size(); textureArrayIndex++)
	{
		VkDescriptorImageInfo& imageBufferInfo = imageInfoList[textureArrayIndex];
		imageBufferInfo.sampler = blockySampler;
		imageBufferInfo.imageView = textureList[textureArrayIndex]->imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	//BIND TLAS
	VkWriteDescriptorSetAccelerationStructureKHR descASInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	descASInfo.accelerationStructureCount = 1;
	descASInfo.pAccelerationStructures = &tlas;

	vkutil::DescriptorBuilder::begin(_engine->_descriptorBindlessLayoutCache.get(), _engine->_descriptorBindlessAllocator.get())
		.bind_buffer(verticesBinding, vertexBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, vertexBufferInfoList.size())
		.bind_image(textureBinding, imageInfoList.data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, imageInfoList.size())
		.bind_rt_as(tlasBinding, &descASInfo, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
		.build_bindless(_bindlessSet, _bindlessSetLayout);
}

void VulkanReSTIRUpdateReservoirPlusShadePass::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	{
		std::array<VkImageMemoryBarrier, 1> outBarriers =
		{
			vkinit::image_barrier(get_output().image._image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, outBarriers.size(), outBarriers.data());
	}

	VkClearValue clear_value = { 0., 0., 0., 0. };
	cmd->clear_image(_outputTex, clear_value);

	{
		std::array<VkImageMemoryBarrier, 1> outBarriers =
		{
			vkinit::image_barrier(get_output().image._image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, 0, 0, 0, outBarriers.size(), outBarriers.data());
	}

	cmd->raytrace(&_rgenRegion, &_missRegion, &_hitRegion, &_callRegion, _imageExtent.width, _imageExtent.height, 1,
		[&](VkCommandBuffer cmd) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipeline(EPipelineType::ReSTIR_UpdateReservoir_PlusShade));
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_UpdateReservoir_PlusShade), 0,
				1, &_bindlessSet, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_UpdateReservoir_PlusShade), 1,
				1, &_globalUniformsDescSet[current_frame_index], 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_UpdateReservoir_PlusShade), 2,
				1, &_rtDescSet[current_frame_index], 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_UpdateReservoir_PlusShade), 3,
				1, &_gBuffDescSet[current_frame_index], 0, nullptr);
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

#endif
