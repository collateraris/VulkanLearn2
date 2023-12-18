#include "vk_raytrace_graphic_pipeline.h"

#if RAYTRACER_ON

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>

void VulkanRaytracingGraphicsPipeline::init(VulkanEngine* engine)
{
	_engine = engine;

	{
		VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		prop2.pNext = &_rtProperties;
		vkGetPhysicalDeviceProperties2(_engine->_chosenPhysicalDeviceGPU, &prop2);
	}

	_imageExtent = {
		_engine->_windowExtent.width,
		_engine->_windowExtent.height,
		1
	};

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_colorTexture = texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_depthTexture = texBuilder.start()
			.make_img_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, _imageExtent)
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT)
			.create_texture();
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::BaseRaytracer,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				uint32_t rayGenIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("raytrace.rgen.spv")), VK_SHADER_STAGE_RAYGEN_BIT_NV);
				uint32_t rayMissIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("raytrace.rmiss.spv")), VK_SHADER_STAGE_MISS_BIT_NV);
				uint32_t rayClosestHitIndex = defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("raytrace.rchit.spv")), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
				defaultEffect.reflect_layout(engine->_device, nullptr, 0);

				RTPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);

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
				group.generalShader = rayMissIndex;
				rtShaderGroups.push_back(group);

				// closest hit shader
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				group.generalShader = VK_SHADER_UNUSED_KHR;
				group.closestHitShader = rayClosestHitIndex;
				rtShaderGroups.push_back(group);

				pipelineBuilder._rayPipelineInfo.groupCount = static_cast<uint32_t>(rtShaderGroups.size());
				pipelineBuilder._rayPipelineInfo.pGroups = rtShaderGroups.data();

				pipelineLayout = pipelineBuilder._pipelineLayout;

				pipeline = pipelineBuilder.build_rt_pipeline(engine->_device);
			});
	}

	{
		uint32_t missCount{ 1 };
		uint32_t hitCount{ 1 };
		auto     handleCount = 1 + missCount + hitCount;
		uint32_t handleSize = _rtProperties.shaderGroupHandleSize;

		// The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
		uint32_t handleSizeAligned = vkutil::align_up(handleSize, _rtProperties.shaderGroupHandleAlignment);

		_rgenRegion.stride = vkutil::align_up(handleSizeAligned, _rtProperties.shaderGroupBaseAlignment);
		_rgenRegion.size = _rgenRegion.stride;  // The size member of pRayGenShaderBindingTable must be equal to its stride member
		_missRegion.stride = handleSizeAligned;
		_missRegion.size = vkutil::align_up(missCount * handleSizeAligned, _rtProperties.shaderGroupBaseAlignment);
		_hitRegion.stride = handleSizeAligned;
		_hitRegion.size = vkutil::align_up(hitCount * handleSizeAligned, _rtProperties.shaderGroupBaseAlignment);

		// Get the shader group handles
		uint32_t             dataSize = handleCount * handleSize;
		std::vector<uint8_t> handles(dataSize);
		auto result = vkGetRayTracingShaderGroupHandlesKHR(_engine->_device, _engine->_renderPipelineManager.get_pipeline(EPipelineType::BaseRaytracer), 0, handleCount, dataSize, handles.data());

		// Allocate a buffer for storing the SBT.
		VkDeviceSize sbtSize = _rgenRegion.size + _missRegion.size + _hitRegion.size + _callRegion.size;
		_rtSBTBuffer = _engine->create_staging_buffer(sbtSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR);

		// Find the SBT addresses of each group
		VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, _rtSBTBuffer._buffer };
		VkDeviceAddress           sbtAddress = vkGetBufferDeviceAddress(_engine->_device, &info);
		_rgenRegion.deviceAddress = sbtAddress;
		_missRegion.deviceAddress = sbtAddress + _rgenRegion.size;
		_hitRegion.deviceAddress = sbtAddress + _rgenRegion.size + _missRegion.size;

		// Helper to retrieve the handle data
		auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

		_engine->map_buffer(_engine->_allocator, _rtSBTBuffer._allocation, [&](void*& data) {
			uint8_t* pSBTBuffer = reinterpret_cast<uint8_t*>(data);
			uint8_t* pData = pSBTBuffer;
			uint32_t handleIdx{ 0 };
			// Raygen
			memcpy(pData, getHandle(handleIdx++), handleSize);
			// Miss
			pData = pSBTBuffer + _rgenRegion.size;
			memcpy(pData, getHandle(handleIdx++), handleSize);
			// Hit
			pData = pSBTBuffer + _rgenRegion.size + _missRegion.size;
			memcpy(pData, getHandle(handleIdx++), handleSize);

			});
	}
}

void VulkanRaytracingGraphicsPipeline::create_blas(const std::vector<std::unique_ptr<Mesh>>& meshList)
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
			size_t bufferSize = mesh->_indices.size() * sizeof(mesh->_indices[0]);

			mesh->_indicesBufferRT = _engine->create_buffer_n_copy_data(bufferSize, mesh->_indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
		}

		allBlas.emplace_back(create_blas_input(*mesh));
	}

	_rtBuilder.build_blas(*_engine, allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void VulkanRaytracingGraphicsPipeline::create_tlas(const std::vector<RenderObject>& renderables)
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

	for (int i = 0; i < FRAME_OVERLAP; i++) 
	{
		// set 0

		VkAccelerationStructureKHR                   tlas = _rtBuilder.get_acceleration_structure();
		VkWriteDescriptorSetAccelerationStructureKHR descASInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		descASInfo.accelerationStructureCount = 1;
		descASInfo.pAccelerationStructures = &tlas;

		VkDescriptorImageInfo outImageBufferInfo;
		outImageBufferInfo.sampler = VK_NULL_HANDLE;
		outImageBufferInfo.imageView = _colorTexture.imageView;
		outImageBufferInfo.imageLayout = _colorTexture.createInfo.initialLayout;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_rt_as(0, &descASInfo, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.bind_image(1, &outImageBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.build(_rtDescSet[i], _rtDescSetLayout);

		//set 1

		_globalUniformsBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanRaytracingGraphicsPipeline::GlobalUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		VkDescriptorBufferInfo globalUniformsInfo;
		globalUniformsInfo.buffer = _globalUniformsBuffer[i]._buffer;
		globalUniformsInfo.offset = 0;
		globalUniformsInfo.range = sizeof(VulkanRaytracingGraphicsPipeline::GlobalUniforms);

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
			.build(_globalUniformsDescSet[i], _globalUniformsDescSetLayout);
	}
}

void VulkanRaytracingGraphicsPipeline::copy_global_uniform_data(VulkanRaytracingGraphicsPipeline::GlobalUniforms& globalData, int current_frame_index)
{

	_engine->map_buffer(_engine->_allocator, _globalUniformsBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &globalData, sizeof(VulkanRaytracingGraphicsPipeline::GlobalUniforms));
		});
}

void VulkanRaytracingGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	cmd->raytrace(&_rgenRegion, &_missRegion, &_hitRegion, &_callRegion, _imageExtent.width, _imageExtent.height, 1,
		[&](VkCommandBuffer cmd){
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipeline(EPipelineType::BaseRaytracer));
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::BaseRaytracer), 0,
				1, &_rtDescSet[current_frame_index], 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::BaseRaytracer), 1,
				1, &_globalUniformsDescSet[current_frame_index], 0, nullptr);
		});
}

const Texture& VulkanRaytracingGraphicsPipeline::get_output() const
{
	return _colorTexture;
}

VulkanRaytracerBuilder::BlasInput VulkanRaytracingGraphicsPipeline::create_blas_input(Mesh& mesh)
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
