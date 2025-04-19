#include <vk_resource_manager.h>
#include <vk_initializers.h>

#include <vk_engine.h>

uint32_t ResourceManager::store_texture(std::string& name)
{
	auto texFound = textureCache.find(name);
	if (texFound == textureCache.end())
	{
		textureCache[name] = std::make_unique<Texture>();
	}

	textureList.push_back(textureCache[name].get());

	return textureList.size() - 1;
}

AllocatedSampler* ResourceManager::create_engine_sampler(ESamplerType sampleNameId)
{
	if (engineSamplerCache[(uint32_t)sampleNameId] == nullptr)
		engineSamplerCache[(uint32_t)sampleNameId] = std::make_unique<AllocatedSampler>();

	return get_engine_sampler(sampleNameId);
}

AllocatedSampler* ResourceManager::get_engine_sampler(ESamplerType sampleNameId)
{
	return engineSamplerCache[(uint32_t)sampleNameId].get();
}


Texture* ResourceManager::create_engine_texture(ETextureResourceNames texNameId)
{
	if (engineTextureCache[(uint32_t)texNameId] == nullptr)
		engineTextureCache[(uint32_t)texNameId] = std::make_unique<Texture>();

	return get_engine_texture(texNameId);
}

Texture* ResourceManager::get_engine_texture(ETextureResourceNames texNameId)
{
	return engineTextureCache[(uint32_t)texNameId].get();
}

AllocateDescriptor* ResourceManager::create_engine_descriptor(EDescriptorResourceNames descrNameId)
{
	if (engineDescriptorCache[(uint32_t)descrNameId] == nullptr)
		engineDescriptorCache[(uint32_t)descrNameId] = std::make_unique<AllocateDescriptor>();

	return get_engine_descriptor(descrNameId);
}

AllocateDescriptor* ResourceManager::get_engine_descriptor(EDescriptorResourceNames descrNameId)
{
	return engineDescriptorCache[(uint32_t)descrNameId].get();
}

void ResourceManager::load_meshes(VulkanEngine* _engine, const std::vector<std::unique_ptr<Mesh>>& meshList)
{
	for (auto& mesh : meshList)
	{
		VkBufferUsageFlags flag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		VkBufferUsageFlags rayTracingFlags =  // used also for building acceleration structures
			flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		{
			size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->_vertices.size() * sizeof(Vertex));
			mesh->_vertexBufferRT = _engine->create_buffer_n_copy_data(bufferSize, mesh->_vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
		}
		{
			size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->_indices.size() * sizeof(uint32_t));
			mesh->_indicesBufferRT = _engine->create_buffer_n_copy_data(bufferSize, mesh->_indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
		}
		{
			std::vector<glm::ivec4> indicesArray;
			uint32_t triangleCount = mesh->_indices.size() / 3;
			indicesArray.reserve(triangleCount);
			for (uint32_t i = 0; i < triangleCount; i++)
			{
				indicesArray.push_back(glm::ivec4(mesh->_indices[i * 3 + 0], mesh->_indices[i * 3 + 1], mesh->_indices[i * 3 + 2], -1.f));
			}

			size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(indicesArray.size() * sizeof(glm::ivec4));
			mesh->_indicesBuffer = _engine->create_buffer_n_copy_data(bufferSize, indicesArray.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}
	mesh->buildMeshlets();

	{
		size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->_meshlets.size() * sizeof(Meshlet));

		mesh->_meshletsBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->_meshlets.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	}

	{
		size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->meshletdata.size() * sizeof(uint32_t));

		mesh->_meshletdataBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->meshletdata.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	}
	}
}

void ResourceManager::load_images(VulkanEngine* _engine, const std::unordered_map<std::string, std::unique_ptr<Texture>>& textureCache)
{
	for (auto& [path, texPtr] : textureCache)
	{
		Texture* tex = texPtr.get();
		VkFormat image_format;
		if (vkutil::load_image_from_file(*_engine, path, *tex, image_format))
		{
			VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(image_format, tex->image._image, VK_IMAGE_ASPECT_COLOR_BIT);
			imageinfo.subresourceRange.levelCount = tex->mipLevels;
			vkCreateImageView(_engine->_device, &imageinfo, nullptr, &tex->imageView);
		}
	}
}

void ResourceManager::init_samplers(VulkanEngine* _engine, ResourceManager& resManager)
{
	if (AllocatedSampler* alloc_sampler = resManager.create_engine_sampler(ESamplerType::NEAREST_REPEAT))
	{
		VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);

		vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &alloc_sampler->sampler);
		alloc_sampler->samplerType = ESamplerType::NEAREST_REPEAT;
	}

	if (AllocatedSampler* alloc_sampler = resManager.create_engine_sampler(ESamplerType::NEAREST_CLAMP))
	{
		VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

		vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &alloc_sampler->sampler);
		alloc_sampler->samplerType = ESamplerType::NEAREST_CLAMP;
	}

	if (AllocatedSampler* alloc_sampler = resManager.create_engine_sampler(ESamplerType::NEAREST_MIRRORED_REPEAT))
	{
		VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);

		vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &alloc_sampler->sampler);
		alloc_sampler->samplerType = ESamplerType::NEAREST_MIRRORED_REPEAT;
	}

	if (AllocatedSampler* alloc_sampler = resManager.create_engine_sampler(ESamplerType::LINEAR_REPEAT))
	{
		VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

		vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &alloc_sampler->sampler);
		alloc_sampler->samplerType = ESamplerType::LINEAR_REPEAT;
	}

	if (AllocatedSampler* alloc_sampler = resManager.create_engine_sampler(ESamplerType::LINEAR_CLAMP))
	{
		VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

		vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &alloc_sampler->sampler);
		alloc_sampler->samplerType = ESamplerType::LINEAR_CLAMP;
	}

	if (AllocatedSampler* alloc_sampler = resManager.create_engine_sampler(ESamplerType::LINEAR_MIRRORED_REPEAT))
	{
		VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);

		vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &alloc_sampler->sampler);
		alloc_sampler->samplerType = ESamplerType::LINEAR_MIRRORED_REPEAT;
	}
}

std::vector<IndirectBatch> ResourceManager::compact_draws(RenderObject* objects, int count)
{
	std::vector<IndirectBatch> draws;

	IndirectBatch firstDraw;
	firstDraw.mesh = objects[0].mesh;
	firstDraw.matDescIndex = objects[0].matDescIndex;
	firstDraw.first = 0;
	firstDraw.count = 1;

	draws.push_back(firstDraw);

	for (int i = 0; i < count; i++)
	{
		//compare the mesh and material with the end of the vector of draws
		bool sameMesh = objects[i].mesh == draws.back().mesh;
		bool sameMaterial = objects[i].matDescIndex == draws.back().matDescIndex;

		if (sameMesh && sameMaterial)
		{
			//all matches, add count
			draws.back().count++;
		}
		else
		{
			//add new draw
			IndirectBatch newDraw;
			newDraw.mesh = objects[i].mesh;
			newDraw.matDescIndex = objects[i].matDescIndex;
			newDraw.transformMatrix = objects[i].transformMatrix;
			newDraw.first = i;
			newDraw.count = 1;

			draws.push_back(newDraw);
		}
	}
	return draws;
}

void ResourceManager::init_scene(VulkanEngine* _engine, ResourceManager& resManager, Scene& scene)
{
	std::unordered_map<int, std::unordered_map<int, std::vector<RenderObject>>> renderablesMap;

	for (int nodeIndex = 1; nodeIndex < scene._hierarchy.size(); nodeIndex++)
	{
		RenderObject map;
		auto it = scene._meshes.find(nodeIndex);
		if (it == scene._meshes.end())
			continue;
		map.meshIndex = scene._meshes[nodeIndex];
		map.mesh = resManager.meshList[map.meshIndex].get();
		map.matDescIndex = scene._matForNode[nodeIndex];
		const std::string& matName = resManager.matDescList[map.matDescIndex].get()->matName;
		map.transformMatrix = scene._localTransforms[nodeIndex];

		renderablesMap[map.meshIndex][map.matDescIndex].push_back(map);
	}

	resManager.renderables.clear();
	for (const auto& [meshIndex, matMap] : renderablesMap)
		for (const auto& [matIndex, mapVector] : matMap)
			for (const auto& map : mapVector)
				resManager.renderables.push_back(map);
	resManager.indirectBatchRO = compact_draws(resManager.renderables.data(), resManager.renderables.size());

	//candidate for emissive triangles
	resManager.emissiveTriangles.push_back(EmissiveInfo{});
	for (RenderObject& object : resManager.renderables)
	{
		if (resManager.matDescList[object.matDescIndex]->emissionTextureIndex >= 0)
		{
			size_t numIndx = resManager.meshList[object.meshIndex]->_indices.size();
			for (size_t i = 0; i < numIndx; i += 3)
			{
				Mesh* mesh = resManager.meshList[object.meshIndex].get();
				uint32_t indx0 = mesh->_indices[i];
				uint32_t indx1 = mesh->_indices[i + 1];
				uint32_t indx2 = mesh->_indices[i + 2];
				glm::vec3 vpos0 = glm::vec3(mesh->_vertices[indx0].positionXYZ_normalX);
				glm::vec3 vpos1 = glm::vec3(mesh->_vertices[indx1].positionXYZ_normalX);
				glm::vec3 vpos2 = glm::vec3(mesh->_vertices[indx2].positionXYZ_normalX);
				EmissiveInfo info;
				info.pos0_ = object.transformMatrix * glm::vec4(vpos0, 1.);
				info.pos1_ = object.transformMatrix * glm::vec4(vpos1, 1.);
				info.pos2_ = object.transformMatrix * glm::vec4(vpos1, 1.);

				resManager.emissiveTriangles.push_back(info);
			}
		}
	}
	uint32_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(resManager.emissiveTriangles.size() * sizeof(EmissiveInfo));
	resManager.emissiveTrianglesBuffer = _engine->create_buffer_n_copy_data(bufferSize, resManager.emissiveTriangles.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	//create global object buffer

	std::vector<GlobalObjectData> objectSSBO;
	objectSSBO.reserve(resManager.renderables.size());
	for (int i = 0; i < resManager.renderables.size(); i++)
	{
		const RenderObject& object = resManager.renderables[i];
		objectSSBO.push_back({
			.model = object.transformMatrix,
			.meshIndex = static_cast<uint32_t>(object.meshIndex),
			.meshletCount = static_cast<uint32_t>(object.mesh->_meshlets.size()),
			.diffuseTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->diffuseTextureIndex,
			.normalTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->normalTextureIndex,
			.metalnessTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->metalnessTextureIndex,
			.roughnessTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->roughnessTextureIndex,
			.emissionTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->emissionTextureIndex,
			.opacityTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->opacityTextureIndex,
			.baseColorFactor = _engine->_resManager.matDescList[object.matDescIndex]->baseColorFactor,
		});
	}

	bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(objectSSBO.size() * sizeof(GlobalObjectData));
	resManager.globalObjectBuffer = _engine->create_buffer_n_copy_data(bufferSize, objectSSBO.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void ResourceManager::init_rt_scene(VulkanEngine* _engine, ResourceManager& resManager)
{	
	auto create_blas_input = [&](Mesh & mesh) -> VulkanRaytracerBuilder::BlasInput
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
		asGeom.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;  // Avoid double hits;
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
	};


	auto create_blas = [&](VulkanRaytracerBuilder& rtBuilder, const std::vector<std::unique_ptr<Mesh>>&meshList)
	{
			std::vector<VulkanRaytracerBuilder::BlasInput> allBlas;
			allBlas.reserve(meshList.size());
			for (auto& mesh : meshList)
			{
				allBlas.emplace_back(create_blas_input(*mesh));
			}

			rtBuilder.build_blas(*_engine, allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
	};

	// Convert a Mat4x4 to the matrix required by acceleration structures
	auto toTransformMatrixKHR = [&](glm::mat4 matrix)
	{
			// VkTransformMatrixKHR uses a row-major memory layout, while glm::mat4
			// uses a column-major memory layout. We transpose the matrix so we can
			// memcpy the matrix's data directly.
			glm::mat4            temp = glm::transpose(matrix);
			VkTransformMatrixKHR out_matrix;
			memcpy(&out_matrix, &temp, sizeof(VkTransformMatrixKHR));
			return out_matrix;
	};

	auto create_tlas = [&](VulkanRaytracerBuilder & rtBuilder, const std::vector<RenderObject>&renderables)
	{
		std::vector<VkAccelerationStructureInstanceKHR> tlas;
		tlas.reserve(renderables.size());
		uint32_t counter = 0;
		for (const auto& inst : renderables)
		{
			VkAccelerationStructureInstanceKHR rayInst{};
			rayInst.transform = toTransformMatrixKHR(inst.transformMatrix);  // Position of the instance
			rayInst.instanceCustomIndex = counter++; // gl_InstanceCustomIndexEXT
			rayInst.accelerationStructureReference = rtBuilder.get_blas_device_address(_engine->_device, inst.meshIndex);
			rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			rayInst.mask = 0xFF;       //  Only be hit if rayMask & instance.mask != 0
			rayInst.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
			tlas.emplace_back(rayInst);
		}
		rtBuilder.build_tlas(*_engine, tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
	};

	create_blas(_engine->_resManager._rtBuilder, _engine->_resManager.meshList);
	create_tlas(_engine->_resManager._rtBuilder, _engine->_resManager.renderables);
}

void ResourceManager::init_global_bindless_descriptor(VulkanEngine* _engine, ResourceManager& resManager)
{
	const uint32_t verticesBinding = 0;
	const uint32_t textureBinding = 1;
	const uint32_t tlasBinding = 2;
	const uint32_t globalObjectBinding = 3;
	const uint32_t meshletsBinding = 4;
	const uint32_t meshletsDataBinding = 5;
	const uint32_t lightBufferBinding = 6;
	const uint32_t indicesBinding = 7;
	const uint32_t blockySamplerBinding = 8;

	const uint32_t irradianceMapBinding = 8;
	const uint32_t prefilteredMapBinding = 9;
	const uint32_t brdfLUTBinding = 10;
	const uint32_t emissiveTriangleBinding = 11;

	const auto& meshList = resManager.meshList;
	const auto& textureList = resManager.textureList;
	const auto& tlas = resManager._rtBuilder.get_acceleration_structure();
	//BIND MESHDATA
	std::vector<VkDescriptorBufferInfo> vertexBufferInfoList{};
	vertexBufferInfoList.resize(meshList.size());

	std::vector<VkDescriptorBufferInfo> meshletBufferInfoList{};
	meshletBufferInfoList.resize(meshList.size());

	std::vector<VkDescriptorBufferInfo> meshletdataBufferInfoList{};
	meshletdataBufferInfoList.resize(meshList.size());

	std::vector<VkDescriptorBufferInfo> indexBufferInfoList{};
	indexBufferInfoList.resize(meshList.size());

	for (uint32_t meshArrayIndex = 0; meshArrayIndex < meshList.size(); meshArrayIndex++)
	{
		Mesh* mesh = meshList[meshArrayIndex].get();

		VkDescriptorBufferInfo& vertexBufferInfo = vertexBufferInfoList[meshArrayIndex];
		vertexBufferInfo.buffer = mesh->_vertexBufferRT._buffer;
		vertexBufferInfo.offset = 0;
		vertexBufferInfo.range = mesh->_vertexBufferRT._size;

		VkDescriptorBufferInfo& meshletBufferInfo = meshletBufferInfoList[meshArrayIndex];
		meshletBufferInfo.buffer = mesh->_meshletsBuffer._buffer;
		meshletBufferInfo.offset = 0;
		meshletBufferInfo.range = mesh->_meshletsBuffer._size;

		VkDescriptorBufferInfo& meshletdataBufferInfo = meshletdataBufferInfoList[meshArrayIndex];
		meshletdataBufferInfo.buffer = mesh->_meshletdataBuffer._buffer;
		meshletdataBufferInfo.offset = 0;
		meshletdataBufferInfo.range = mesh->_meshletdataBuffer._size;
		VkDescriptorBufferInfo& indexBufferInfo = indexBufferInfoList[meshArrayIndex];
		indexBufferInfo.buffer = mesh->_indicesBuffer._buffer;
		indexBufferInfo.offset = 0;
		indexBufferInfo.range = mesh->_indicesBuffer._size;
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

	VkDescriptorBufferInfo objectBufferInfo;
	objectBufferInfo.buffer = resManager.globalObjectBuffer._buffer;
	objectBufferInfo.offset = 0;
	objectBufferInfo.range = resManager.globalObjectBuffer._size;

	VkDescriptorBufferInfo emissiveBufferInfo;
	emissiveBufferInfo.buffer = resManager.emissiveTrianglesBuffer._buffer;
	emissiveBufferInfo.offset = 0;
	emissiveBufferInfo.range = resManager.emissiveTrianglesBuffer._size;

	VkDescriptorBufferInfo lightsInfo;
	lightsInfo.buffer = _engine->_lightManager.get_light_buffer()._buffer;
	lightsInfo.offset = 0;
	lightsInfo.range = _engine->_lightManager.get_light_buffer()._size;

	VkSampler& sampler = _engine->get_engine_sampler(ESamplerType::NEAREST_REPEAT)->sampler;

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

	VkDescriptorImageInfo blockySamplerInfo;
	blockySamplerInfo.sampler = blockySampler;
	blockySamplerInfo.imageView = nullptr;
	blockySamplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vkutil::DescriptorBuilder::begin(_engine->_descriptorBindlessLayoutCache.get(), _engine->_descriptorBindlessAllocator.get())
		.bind_buffer(verticesBinding, vertexBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT, vertexBufferInfoList.size())
		.bind_image(textureBinding, imageInfoList.data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT, imageInfoList.size())
		.bind_rt_as(tlasBinding, &descASInfo, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT)
		.bind_buffer(globalObjectBinding, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT)
		.bind_buffer(meshletsBinding, meshletBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MESH_BIT_NV, meshletBufferInfoList.size())
		.bind_buffer(meshletsDataBinding, meshletdataBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MESH_BIT_NV, meshletdataBufferInfoList.size())
		.bind_buffer(lightBufferBinding, &lightsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_buffer(indicesBinding, indexBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT, indexBufferInfoList.size())
		.bind_image(irradianceMapBinding, &irradMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(prefilteredMapBinding, &prefilteredMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(brdfLUTBinding, &brdflutMapImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.bind_image(blockySamplerBinding, &blockySamplerInfo, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV | VK_SHADER_STAGE_FRAGMENT_BIT)
		.bind_buffer(emissiveTriangleBinding, &emissiveBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		.build_bindless(_engine, EDescriptorResourceNames::Bindless_Scene);
}
