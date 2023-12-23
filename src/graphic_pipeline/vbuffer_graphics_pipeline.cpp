#include <graphic_pipeline/vbuffer_graphics_pipeline.h>

#if VBUFFER_ON

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_initializers.h>

void VulkanVbufferGraphicsPipeline::init(VulkanEngine* engine)
{
	_engine = engine;

	_vbufferExtent = {
		_engine->_windowExtent.width,
		_engine->_windowExtent.height,
		1
	};

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_vbufferTex = texBuilder.start()
			.make_img_info(_vbufferFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, _vbufferExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_vbufferFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_depthTexture = texBuilder.start()
			.make_img_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, _vbufferExtent)
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT)
			.create_texture();
	}

	{
		init_scene_buffer(_engine->_renderables, _engine->_resManager.meshList);
		init_bindless(_engine->_resManager.meshList);
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::Bindless_TaskMeshIndirectForward,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("vbuffer_generate.task.spv")), VK_SHADER_STAGE_TASK_BIT_EXT);
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("vbuffer_generate.mesh.spv")), VK_SHADER_STAGE_MESH_BIT_EXT);
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("vbuffer_generate.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

				defaultEffect.reflect_layout(_engine->_device, nullptr, 0);
				//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
				GraphicPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);
				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _bindlessSetLayout, _globalDescSetLayout, _vbufferDescSetLayout };
				mesh_pipeline_layout_info.setLayoutCount = setLayout.size();
				mesh_pipeline_layout_info.pSetLayouts = setLayout.data();

				vkCreatePipelineLayout(_engine->_device, &mesh_pipeline_layout_info, nullptr, &pipelineBuilder._pipelineLayout);
				VkPipelineLayout meshPipLayout = pipelineBuilder._pipelineLayout;

				//vertex input controls how to read vertices from vertex buffers. We arent using it yet
				pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

				//input assembly is the configuration for drawing triangle lists, strips, or individual points.
				//we are just going to draw triangle list
				pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

				//build viewport and scissor from the swapchain extents
				pipelineBuilder._viewport.x = 0.0f;
				pipelineBuilder._viewport.y = 0.0f;
				pipelineBuilder._viewport.width = _vbufferExtent.width;
				pipelineBuilder._viewport.height = _vbufferExtent.height;
				pipelineBuilder._viewport.minDepth = 0.0f;
				pipelineBuilder._viewport.maxDepth = 1.0f;

				pipelineBuilder._scissor.offset = { 0, 0 };
				pipelineBuilder._scissor.extent = VkExtent2D(_vbufferExtent.width, _vbufferExtent.height);

				//configure the rasterizer to draw filled triangles
				pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

				//we dont use multisampling, so just run the default one
				pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

				//a single blend attachment with no blending and writing to RGBA
				pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

				//default depthtesting
				pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

				VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(_engine->_device,
					_engine->_renderPassManager.get_render_pass(ERenderPassType::Default)->get_render_pass());

				pipeline = meshPipeline;
				pipelineLayout = meshPipLayout;
			});
	}
}

void VulkanVbufferGraphicsPipeline::copy_global_uniform_data(VulkanVbufferGraphicsPipeline::SGlobalCamera& camData, int current_frame_index)
{
	_engine->map_buffer(_engine->_allocator, _globalCameraBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &camData, sizeof(VulkanVbufferGraphicsPipeline::SGlobalCamera));
		});
}

void VulkanVbufferGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	VkDeviceSize indirect_offset = 0 * sizeof(VkDrawMeshTasksIndirectCommandNV);
	uint32_t draw_stride = sizeof(VkDrawMeshTasksIndirectCommandNV);

	cmd->draw_mesh_tasks_indirect(_indirectBuffer, indirect_offset, _objectsSize, draw_stride,
		[&](VkCommandBuffer cmd) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipeline(EPipelineType::VbufferGenerate));

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::VbufferGenerate), 0, 1, &_bindlessSet, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::VbufferGenerate), 1, 1, &_globalDescSet[current_frame_index], 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::VbufferGenerate), 2, 1, &_vbufferDescSet[current_frame_index], 0, nullptr);
	});
}

const Texture& VulkanVbufferGraphicsPipeline::get_vbuffer_output() const
{
	return _vbufferTex;
}

void VulkanVbufferGraphicsPipeline::init_scene_buffer(const std::vector<RenderObject>& renderables, const std::vector<std::unique_ptr<Mesh>>& meshList)
{
	for (auto& mesh : meshList)
	{
		mesh->remapVertexToVertexMS()
			.buildMeshlets();
		mesh->remapVertexMSToVertexVisB();

		{
			size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->_verticesVisB.size() * sizeof(Vertex_VisB));

			mesh->_vertexBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->_verticesVisB.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}
		{
			size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->_meshlets.size() * sizeof(Meshlet));

			mesh->_meshletsBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->_meshlets.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}

		{
			size_t bufferSize = _engine->padSizeToMinStorageBufferOffsetAlignment(mesh->meshletdata.size() * sizeof(uint32_t));

			mesh->_meshletdataBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->meshletdata.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}
	}

	_objectsSize = renderables.size();
	_objectBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanVbufferGraphicsPipeline::ObjectData) * renderables.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VkDescriptorImageInfo outImageBufferInfo;
		outImageBufferInfo.sampler = VK_NULL_HANDLE;
		outImageBufferInfo.imageView = _vbufferTex.imageView;
		outImageBufferInfo.imageLayout = _vbufferTex.createInfo.initialLayout;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_image(0, &outImageBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(_vbufferDescSet[i], _vbufferDescSetLayout);

		_globalCameraBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanVbufferGraphicsPipeline::SGlobalCamera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		VkDescriptorBufferInfo globalUniformsInfo;
		globalUniformsInfo.buffer = _globalCameraBuffer[i]._buffer;
		globalUniformsInfo.offset = 0;
		globalUniformsInfo.range = _globalCameraBuffer[i]._size;

		VkDescriptorBufferInfo objectBufferInfo;
		objectBufferInfo.buffer = _objectBuffer._buffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = _objectBuffer._size;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
			.bind_buffer(1, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
			.build(_globalDescSet[i], _globalDescSetLayout);
	}

	_engine->map_buffer(_engine->_allocator, _objectBuffer._allocation, [&](void*& data) {
		VulkanVbufferGraphicsPipeline::ObjectData* objectSSBO = (VulkanVbufferGraphicsPipeline::ObjectData*)data;

		for (int i = 0; i < renderables.size(); i++)
		{
			const RenderObject& object = renderables[i];
			objectSSBO[i].meshIndex = object.meshIndex;
			objectSSBO[i].meshletCount = object.mesh->_meshlets.size();
		}
		});

	_indirectBuffer = _engine->create_cpu_to_gpu_buffer(MAX_COMMANDS * sizeof(VkDrawMeshTasksIndirectCommandNV), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

	_engine->map_buffer(_engine->_allocator, _indirectBuffer._allocation, [&](void*& data) {
			VkDrawMeshTasksIndirectCommandNV* drawCommands = (VkDrawMeshTasksIndirectCommandNV*)data;
			//encode the draw data of each object into the indirect draw buffer
			for (int i = 0; i < renderables.size(); i++)
			{
				const RenderObject& object = renderables[i];
				drawCommands[i].taskCount = (object.mesh->_meshlets.size() + 31 ) / 32;
				drawCommands[i].firstTask = 0;
			}
		});
}

void VulkanVbufferGraphicsPipeline::init_bindless(const std::vector<std::unique_ptr<Mesh>>& meshList)
{
	const uint32_t meshletsVerticesBinding = 0;
	const uint32_t meshletsBinding = 1;
	const uint32_t meshletsDataBinding = 2;

	//BIND MESHDATA
	std::vector<VkDescriptorBufferInfo> vertexBufferInfoList{};
	vertexBufferInfoList.resize(meshList.size());

	std::vector<VkDescriptorBufferInfo> meshletBufferInfoList{};
	meshletBufferInfoList.resize(meshList.size());

	std::vector<VkDescriptorBufferInfo> meshletdataBufferInfoList{};
	meshletdataBufferInfoList.resize(meshList.size());

	for (uint32_t meshArrayIndex = 0; meshArrayIndex < meshList.size(); meshArrayIndex++)
	{
		Mesh* mesh = meshList[meshArrayIndex].get();

		VkDescriptorBufferInfo& vertexBufferInfo = vertexBufferInfoList[meshArrayIndex];
		vertexBufferInfo.buffer = mesh->_vertexBuffer._buffer;
		vertexBufferInfo.offset = 0;
		vertexBufferInfo.range = mesh->_vertexBuffer._size;

		VkDescriptorBufferInfo& meshletBufferInfo = meshletBufferInfoList[meshArrayIndex];
		meshletBufferInfo.buffer = mesh->_meshletsBuffer._buffer;
		meshletBufferInfo.offset = 0;
		meshletBufferInfo.range = mesh->_meshletsBuffer._size;

		VkDescriptorBufferInfo& meshletdataBufferInfo = meshletdataBufferInfoList[meshArrayIndex];
		meshletdataBufferInfo.buffer = mesh->_meshletdataBuffer._buffer;
		meshletdataBufferInfo.offset = 0;
		meshletdataBufferInfo.range = mesh->_meshletdataBuffer._size;
	}

	vkutil::DescriptorBuilder::begin(_engine->_descriptorBindlessLayoutCache.get(), _engine->_descriptorBindlessAllocator.get())
		.bind_buffer(meshletsVerticesBinding, vertexBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV, vertexBufferInfoList.size())
		.bind_buffer(meshletsBinding, meshletBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV, meshletBufferInfoList.size())
		.bind_buffer(meshletsDataBinding, meshletdataBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV, meshletdataBufferInfoList.size())
		.build_bindless(_bindlessSet, _bindlessSetLayout);
}

#endif
