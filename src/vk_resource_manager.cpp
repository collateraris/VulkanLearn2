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
#if GI_RAYTRACER_ON
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
#endif
#if MESHSHADER_ON || GBUFFER_ON || VBUFFER_ON
	mesh->buildMeshlets();

	{
		size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->_meshlets.size() * sizeof(Meshlet));

		mesh->_meshletsBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->_meshlets.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	}

	{
		size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->meshletdata.size() * sizeof(uint32_t));

		mesh->_meshletdataBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->meshletdata.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	}
#endif
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

	//create global object buffer

	resManager.globalObjectBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(GlobalObjectData) * resManager.renderables.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	_engine->map_buffer(_engine->_allocator, resManager.globalObjectBuffer._allocation, [&](void*& data) {
		GlobalObjectData* objectSSBO = (GlobalObjectData*)data;

		for (int i = 0; i < resManager.renderables.size(); i++)
		{
			const RenderObject& object = resManager.renderables[i];
			objectSSBO[i].model = object.transformMatrix;
			objectSSBO[i].meshIndex = object.meshIndex;
			objectSSBO[i].diffuseTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->diffuseTextureIndex;
			objectSSBO[i].normalTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->normalTextureIndex;
			objectSSBO[i].metalnessTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->metalnessTextureIndex;
			objectSSBO[i].roughnessTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->roughnessTextureIndex;
			objectSSBO[i].emissionTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->emissionTextureIndex;
			objectSSBO[i].opacityTexIndex = _engine->_resManager.matDescList[object.matDescIndex]->opacityTextureIndex;
		}
	});
}
