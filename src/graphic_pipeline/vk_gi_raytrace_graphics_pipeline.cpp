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

void VulkanGIShadowsRaytracingGraphicsPipeline::init(VulkanEngine* engine)
{
	_engine = engine;

	{
		create_blas(_engine->_resManager.meshList);
		create_tlas(_engine->_resManager.renderables);
		init_global_buffers();
		init_scene_descriptors(_engine->_resManager.meshList, _engine->_resManager.textureList, _rtBuilder.get_acceleration_structure());
	}

	_restirInitTemporalGP = std::make_unique<VulkanReSTIRInitPlusTemporalPass>();
	_restirInitTemporalGP->init(engine);

	_restirSpacialGP = std::make_unique<VulkanReSTIRSpaceReusePass>();
	_restirSpacialGP->init(engine);

	_restirUpdateShadeGP = std::make_unique<VulkanReSTIRUpdateReservoirPlusShadePass>();
	_restirUpdateShadeGP->init(engine);

	_accumulationGP = std::make_unique<VulkanSimpleAccumulationGraphicsPipeline>();
	_accumulationGP->init(engine, _restirUpdateShadeGP->get_output());

	//_denoiserPass = std::make_unique<VulkanRaytracerDenoiserPass>();
	//_denoiserPass->init(engine);
}

void VulkanGIShadowsRaytracingGraphicsPipeline::create_blas(const std::vector<std::unique_ptr<Mesh>>& meshList)
{
	std::vector<VulkanRaytracerBuilder::BlasInput> allBlas;
	allBlas.reserve(meshList.size());
	for (auto& mesh : meshList)
	{
		allBlas.emplace_back(create_blas_input(*mesh));
	}

	_rtBuilder.build_blas(*_engine, allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void VulkanGIShadowsRaytracingGraphicsPipeline::create_tlas(const std::vector<RenderObject>& renderables)
{
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	tlas.reserve(renderables.size());
	for (const auto& inst : renderables)
	{
		VkAccelerationStructureInstanceKHR rayInst{};
		VkTransformMatrixKHR out_matrix;
		memcpy(&out_matrix, &inst.transformMatrix, sizeof(VkTransformMatrixKHR));
		rayInst.transform = out_matrix;  // Position of the instance
		rayInst.instanceCustomIndex = inst.meshIndex; // gl_InstanceCustomIndexEXT
		rayInst.accelerationStructureReference = _rtBuilder.get_blas_device_address(_engine->_device, inst.meshIndex);
		rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		rayInst.mask = 0xFF;       //  Only be hit if rayMask & instance.mask != 0
		rayInst.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
		tlas.emplace_back(rayInst);
	}
	_rtBuilder.build_tlas(*_engine, tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

}

void VulkanGIShadowsRaytracingGraphicsPipeline::init_scene_descriptors(const std::vector<std::unique_ptr<Mesh>>& meshList, const std::vector<Texture*>& textureList, VkAccelerationStructureKHR  tlas)
{
	VkSampler& sampler = _engine->get_engine_sampler(ESamplerType::NEAREST_REPEAT)->sampler;

	{
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
			.bind_image(0, &irradMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(1, &prefilteredMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(2, &brdflutMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.build(_engine, EDescriptorResourceNames::IBL);
	}

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
			vertexBufferInfo.range = VK_WHOLE_SIZE;
		}

		//BIND SAMPLERS
		VkSampler& blockySampler = _engine->get_engine_sampler(ESamplerType::LINEAR_REPEAT)->sampler;

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
			.bind_buffer(verticesBinding, vertexBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, vertexBufferInfoList.size())
			.bind_image(textureBinding, imageInfoList.data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, imageInfoList.size())
			.bind_rt_as(tlasBinding, &descASInfo, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
			.build_bindless(_engine, EDescriptorResourceNames::Bindless_Scene);
	}

	{
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			//set 1

			VkDescriptorBufferInfo globalUniformsInfo;
			globalUniformsInfo.buffer = _globalUniformsBuffer[i]._buffer;
			globalUniformsInfo.offset = 0;
			globalUniformsInfo.range = VK_WHOLE_SIZE;

			VkDescriptorBufferInfo lightsInfo;
			lightsInfo.buffer = _engine->_lightManager.get_light_buffer(i)._buffer;
			lightsInfo.offset = 0;
			lightsInfo.range = VK_WHOLE_SIZE;

			VkDescriptorBufferInfo objectBufferInfo;
			objectBufferInfo.buffer = _engine->_resManager.globalObjectBuffer._buffer;
			objectBufferInfo.offset = 0;
			objectBufferInfo.range = VK_WHOLE_SIZE;

			VkDescriptorImageInfo vbufferImageBufferInfo;
			vbufferImageBufferInfo.sampler = sampler;
			vbufferImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::VBUFFER)->imageView;
			vbufferImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			EDescriptorResourceNames currentDesciptor = i == 0 ? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0 : EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;


			vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
				.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
				.bind_buffer(1, &lightsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
				.bind_buffer(2, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
				.bind_image(3, &vbufferImageBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
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

	_restirInitTemporalGP->draw(cmd, current_frame_index);
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

VulkanRaytracerBuilder::BlasInput VulkanGIShadowsRaytracingGraphicsPipeline::create_blas_input(Mesh& mesh)
{
	// BLAS builder requires raw device addresses.
	VkDeviceAddress vertexAddress = _engine->get_buffer_device_address(mesh._vertexBufferRT._buffer);
	VkDeviceAddress indexAddress = _engine->get_buffer_device_address(mesh._indicesBufferRT._buffer);

	uint32_t maxPrimitiveCount = mesh._indices.size() / 3;

	// Describe buffer.
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
	triangles.vertexData.deviceAddress = vertexAddress;
	triangles.vertexStride = sizeof(Vertex);
	// Describe index data (32-bit unsigned int)
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	// Indicate identity transform by setting transformData to null device pointer.
	//triangles.transformData = {};
	triangles.maxVertex = mesh._vertices.size() - 1;

	// Identify the above data as containing opaque triangles.
	VkAccelerationStructureGeometryKHR asGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	asGeom.geometry.triangles = triangles;

	// The entire array will be used to build the BLAS.
	VkAccelerationStructureBuildRangeInfoKHR offset;
	offset.firstVertex = 0;
	offset.primitiveCount = maxPrimitiveCount;
	offset.primitiveOffset = 0;
	offset.transformOffset = 0;

	// Our blas is made from only one geometry, but could be made of many geometries
	VulkanRaytracerBuilder::BlasInput input;
	input.asGeometry.emplace_back(asGeom);
	input.asBuildOffsetInfo.emplace_back(offset);

	return input;
}

#endif
