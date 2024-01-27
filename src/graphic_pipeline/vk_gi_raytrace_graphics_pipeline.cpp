#include <graphic_pipeline/vk_gi_raytrace_graphics_pipeline.h>

#if GI_RAYTRACER_ON && GBUFFER_ON

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
		create_tlas(_engine->_renderables);
		init_global_buffers(_engine->_renderables);
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
}

void VulkanGIShadowsRaytracingGraphicsPipeline::create_blas(const std::vector<std::unique_ptr<Mesh>>& meshList)
{
	std::vector<VulkanRaytracerBuilder::BlasInput> allBlas;
	allBlas.reserve(meshList.size());
	for (auto& mesh : meshList)
	{
		VkBufferUsageFlags flag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VkBufferUsageFlags rayTracingFlags =  // used also for building acceleration structures
			flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		{
			size_t bufferSize = mesh->_vertices.size() * sizeof(Vertex);

			mesh->_vertexBufferRT = _engine->create_buffer_n_copy_data(bufferSize, mesh->_vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
		}
		{
			size_t bufferSize = mesh->_indices.size() * sizeof(uint32_t);

			mesh->_indicesBufferRT = _engine->create_buffer_n_copy_data(bufferSize, mesh->_indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
		}

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
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSampler sampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &sampler);

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
			.build(_engine, EDescriptorResourceNames::GBUFFER_IBL);
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
			.build_bindless(_engine, EDescriptorResourceNames::Bindless_Scene);
	}

	{
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			//set 1

			VkDescriptorBufferInfo globalUniformsInfo;
			globalUniformsInfo.buffer = _globalUniformsBuffer[i]._buffer;
			globalUniformsInfo.offset = 0;
			globalUniformsInfo.range = _globalUniformsBuffer[i]._size;

			VkDescriptorBufferInfo lightsInfo;
			lightsInfo.buffer = _engine->_lightManager.get_light_buffer(i)._buffer;
			lightsInfo.offset = 0;
			lightsInfo.range = _engine->_lightManager.get_light_buffer(i)._size;

			VkDescriptorBufferInfo objectBufferInfo;
			objectBufferInfo.buffer = _objectBuffer._buffer;
			objectBufferInfo.offset = 0;
			objectBufferInfo.range = _objectBuffer._size;

			EDescriptorResourceNames currentDesciptor = i == 0 ? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0 : EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;


			vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
				.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
				.bind_buffer(1, &lightsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
				.bind_buffer(2, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV)
				.build(_engine, currentDesciptor);
		}
	}
}

void VulkanGIShadowsRaytracingGraphicsPipeline::init_global_buffers(const std::vector<RenderObject>& renderables)
{
	_objectBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanGIShadowsRaytracingGraphicsPipeline::ObjectData) * renderables.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	_engine->map_buffer(_engine->_allocator, _objectBuffer._allocation, [&](void*& data) {
		VulkanGIShadowsRaytracingGraphicsPipeline::ObjectData* objectSSBO = (VulkanGIShadowsRaytracingGraphicsPipeline::ObjectData*)data;

		for (int i = 0; i < renderables.size(); i++)
		{
			const RenderObject& object = renderables[i];
			objectSSBO[i].meshIndex = object.meshIndex;
			objectSSBO[i].diffuseTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->diffuseTextureIndex;
			objectSSBO[i].normalTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->normalTextureIndex;
			objectSSBO[i].metalnessTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->metalnessTextureIndex;
			objectSSBO[i].roughnessTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->roughnessTextureIndex;
			objectSSBO[i].emissionTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->emissionTextureIndex;
			objectSSBO[i].opacityTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->opacityTextureIndex;
		}
		});

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
	_restirInitTemporalGP->draw(cmd, current_frame_index);

	_restirInitTemporalGP->reservoirCurrTex_barrier_for_raytrace_read(cmd);
	_restirSpacialGP->draw(cmd, current_frame_index);
	_restirInitTemporalGP->reservoirCurrTex_barrier_for_raytrace_write(cmd);

	_restirInitTemporalGP->indirectOutput_barrier_for_raytrace_read(cmd);
	_restirSpacialGP->reservoirSpacial_barrier_for_raytrace_read(cmd);
	_restirInitTemporalGP->reservoirPrevTex_barrier_for_raytrace_write(cmd);
	_restirUpdateShadeGP->draw(cmd, current_frame_index);
	_restirInitTemporalGP->reservoirPrevTex_barrier_for_raytrace_read(cmd);
	_restirInitTemporalGP->indirectOutput_barrier_for_raytrace_write(cmd);
	_restirSpacialGP->reservoirSpacial_barrier_for_raytrace_write(cmd);

	_restirUpdateShadeGP->barrier_for_frag_read(cmd);
	_accumulationGP->draw(cmd, current_frame_index);
	_restirUpdateShadeGP->barrier_for_raytrace_write(cmd);
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
