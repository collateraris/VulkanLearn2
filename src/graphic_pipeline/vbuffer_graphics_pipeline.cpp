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
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_vbufferFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_depthTexture = texBuilder.start()
			.make_img_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, _vbufferExtent)
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT)
			.create_texture();
	}

	{
		RenderPassInfo default_rp;
		default_rp.op_flags = RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
		default_rp.clear_attachments = 1 << 0;
		default_rp.store_attachments = 1 << 0;
		default_rp.num_color_attachments = 1;
		default_rp.color_attachments[0] = &_vbufferTex;
		default_rp.depth_stencil = &_depthTexture;

		RenderPassInfo::Subpass subpass = {};
		subpass.num_color_attachments = 1;
		subpass.depth_stencil_mode = RenderPassInfo::DepthStencil::ReadWrite;
		subpass.color_attachments[0] = 0;
		subpass.resolve_attachments[0] = 1;

		default_rp.num_subpasses = 1;
		default_rp.subpasses = &subpass;

		_engine->_renderPassManager.get_render_pass(ERenderPassType::VisBufferGenerate)->init(_engine, default_rp);
	}

	{
		init_scene_buffer(_engine->_renderables, _engine->_resManager.meshList);
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::VbufferGenerate,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("vbuffer_generate.vert.spv")), VK_SHADER_STAGE_VERTEX_BIT);
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("vbuffer_generate.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

				defaultEffect.reflect_layout(_engine->_device, nullptr, 0);
				//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
				GraphicPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);
				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _globalDescSetLayout };
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

				pipelineBuilder.vertexDescription = Vertex::get_vertex_description();
				pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
				//connect the pipeline builder vertex input info to the one we get from Vertex
				pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = pipelineBuilder.vertexDescription.attributes.data();
				pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)pipelineBuilder.vertexDescription.attributes.size();

				pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = pipelineBuilder.vertexDescription.bindings.data();
				pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)pipelineBuilder.vertexDescription.bindings.size();

				VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(_engine->_device,
					_engine->_renderPassManager.get_render_pass(ERenderPassType::VisBufferGenerate)->get_render_pass());

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

	vkCmdBindPipeline(cmd->get_cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipeline(EPipelineType::VbufferGenerate));

	vkCmdBindDescriptorSets(cmd->get_cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::VbufferGenerate), 0, 1, &_globalDescSet[current_frame_index], 0, nullptr);
	
	Mesh* lastMesh = nullptr;

	for (IndirectBatch& object : _engine->_indirectBatchRO)
	{
		//only bind the mesh if its a different one from last bind
		if (object.mesh != lastMesh) {
			//bind the mesh vertex buffer with offset 0
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd->get_cmd(), 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
			vkCmdBindIndexBuffer(cmd->get_cmd(), object.mesh->_indicesBuffer._buffer, offset, VK_INDEX_TYPE_UINT32);
			lastMesh = object.mesh;
		}

		VkDeviceSize indirect_offset = object.first * sizeof(VkDrawIndexedIndirectCommand);
		uint32_t draw_stride = sizeof(VkDrawIndexedIndirectCommand);

		//execute the draw command buffer on each section as defined by the array of draws
		vkCmdDrawIndexedIndirect(cmd->get_cmd(), _indirectBuffer._buffer, indirect_offset, object.count, draw_stride);
	}
}

const Texture& VulkanVbufferGraphicsPipeline::get_vbuffer_output() const
{
	return _vbufferTex;
}

const Texture& VulkanVbufferGraphicsPipeline::get_depth_output() const
{
	return _depthTexture;
}

void VulkanVbufferGraphicsPipeline::init_scene_buffer(const std::vector<RenderObject>& renderables, const std::vector<std::unique_ptr<Mesh>>& meshList)
{
	for (auto& mesh : meshList)
	{
		{
			size_t bufferSize = mesh->_vertices.size() * sizeof(Vertex);

			mesh->_vertexBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->_vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		}
		{
			size_t bufferSize = mesh->_indices.size() * sizeof(mesh->_indices[0]);

			mesh->_indicesBuffer = _engine->create_buffer_n_copy_data(bufferSize, mesh->_indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		}
	}

	_objectsSize = renderables.size();
	_objectBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanVbufferGraphicsPipeline::ObjectData) * renderables.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
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
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.bind_buffer(1, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build(_globalDescSet[i], _globalDescSetLayout);
	}

	_engine->map_buffer(_engine->_allocator, _objectBuffer._allocation, [&](void*& data) {
		VulkanVbufferGraphicsPipeline::ObjectData* objectSSBO = (VulkanVbufferGraphicsPipeline::ObjectData*)data;

		for (int i = 0; i < renderables.size(); i++)
		{
			const RenderObject& object = renderables[i];
			objectSSBO[i].model = object.transformMatrix;
		}
		});

	_indirectBuffer = _engine->create_cpu_to_gpu_buffer(MAX_COMMANDS * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

	_engine->map_buffer(_engine->_allocator, _indirectBuffer._allocation, [&](void*& data) {
		VkDrawIndexedIndirectCommand* drawCommands = (VkDrawIndexedIndirectCommand*)data;
		//encode the draw data of each object into the indirect draw buffer
		for (int i = 0; i < _engine->_renderables.size(); i++)
		{
			RenderObject& object = _engine->_renderables[i];
			drawCommands[i].indexCount = object.mesh->_indices.size();
			drawCommands[i].instanceCount = 1;
			drawCommands[i].vertexOffset = 0;
			drawCommands[i].firstIndex = 0;
			drawCommands[i].firstInstance = i; //used to access object matrix in the shader
		}
		});
}

#endif
