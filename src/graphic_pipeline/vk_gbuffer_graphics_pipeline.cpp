#include "vk_gbuffer_graphics_pipeline.h"

#if GBUFFER_ON

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_initializers.h>

void VulkanGbufferGenerateGraphicsPipeline::init(VulkanEngine* engine)
{
	_engine = engine;

	init_gbuffer_tex();
	init_render_pass();
	init_scene_buffer(_engine->_resManager.renderables);

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::GBufferGenerate,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("gbuffer_generate.task.spv")), VK_SHADER_STAGE_TASK_BIT_NV);
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("gbuffer_generate.mesh.spv")), VK_SHADER_STAGE_MESH_BIT_NV);
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("gbuffer_generate.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

				defaultEffect.reflect_layout(_engine->_device, nullptr, 0);
				//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
				GraphicPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);
				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->setLayout, _globalDescSetLayout };
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
				pipelineBuilder._viewport.width = _texExtent.width;
				pipelineBuilder._viewport.height = _texExtent.height;
				pipelineBuilder._viewport.minDepth = 0.0f;
				pipelineBuilder._viewport.maxDepth = 1.0f;

				pipelineBuilder._scissor.offset = { 0, 0 };
				pipelineBuilder._scissor.extent = VkExtent2D(_texExtent.width, _texExtent.height);

				//configure the rasterizer to draw filled triangles
				pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

				//we dont use multisampling, so just run the default one
				pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

				pipelineBuilder.attachment_count = 4;
				//a single blend attachment with no blending and writing to RGBA
				//
				// for WPOS
				pipelineBuilder._colorBlendAttachment.push_back(vkinit::color_blend_attachment_state());
				// for NORM
				pipelineBuilder._colorBlendAttachment.push_back(vkinit::color_blend_attachment_state());
				// for UV
				{
					auto colorBlend = vkinit::color_blend_attachment_state();
					colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
					pipelineBuilder._colorBlendAttachment.push_back(vkinit::color_blend_attachment_state());
				}
				// for OBJ_ID
				{
					auto colorBlend = vkinit::color_blend_attachment_state();
					colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
					pipelineBuilder._colorBlendAttachment.push_back(vkinit::color_blend_attachment_state());
				}

				//default depthtesting
				pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

				VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(_engine->_device,
					_engine->_renderPassManager.get_render_pass(ERenderPassType::GBufferGenerate)->get_render_pass());

				pipeline = meshPipeline;
				pipelineLayout = meshPipLayout;
			});
	}
}

void VulkanGbufferGenerateGraphicsPipeline::copy_global_uniform_data(VulkanGbufferGenerateGraphicsPipeline::SGlobalCamera& camData, int current_frame_index)
{
	_engine->map_buffer(_engine->_allocator, _globalCameraBuffer[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &camData, sizeof(VulkanGbufferGenerateGraphicsPipeline::SGlobalCamera));
		});
}

void VulkanGbufferGenerateGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	//make a clear-color from frame number. This will flash with a 120*pi frame period.
	VkClearValue clearValue;
	clearValue.color = { { -1.0f, 0.0f, 0.f, 0.0f } };

	//clear depth at 1
	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.f;

	//start the main renderpass. 
	//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_engine->_renderPassManager.get_render_pass(ERenderPassType::GBufferGenerate)->get_render_pass(), VkExtent2D(_texExtent.width, _texExtent.height), _gBufFramebuffer);

	//connect clear values
	rpInfo.clearValueCount = 5;

	VkClearValue clearValues[] = { clearValue, clearValue,clearValue,clearValue,depthClear };

	rpInfo.pClearValues = &clearValues[0];

	vkCmdBeginRenderPass(cmd->get_cmd(), &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)_texExtent.width;
	viewport.height = (float)_texExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset = { 0, 0 };
	scissor.extent = VkExtent2D(_texExtent.width, _texExtent.height);

	vkCmdSetViewport(cmd->get_cmd(), 0, 1, &viewport);
	vkCmdSetScissor(cmd->get_cmd(), 0, 1, &scissor);
	vkCmdSetDepthBias(cmd->get_cmd(), 0, 0, 0);


	VkDeviceSize indirect_offset = 0 * sizeof(VkDrawMeshTasksIndirectCommandNV);
	uint32_t draw_stride = sizeof(VkDrawMeshTasksIndirectCommandNV);

	cmd->draw_mesh_tasks_indirect(_indirectBuffer, indirect_offset, _objectsSize, draw_stride,
		[&](VkCommandBuffer cmd) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipeline(EPipelineType::GBufferGenerate));

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::GBufferGenerate), 0, 1, &_engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->set, 0, nullptr);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::GBufferGenerate), 1, 1, &_globalDescSet[current_frame_index], 0, nullptr);
		});

	//finalize the render pass
	vkCmdEndRenderPass(cmd->get_cmd());
}

void VulkanGbufferGenerateGraphicsPipeline::barrier_for_gbuffer_shading(VulkanCommandBuffer* cmd)
{
	std::array<VkImageMemoryBarrier, 4> gBufBarriers =
	{
		vkinit::image_barrier(get_wpos_output().image._image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		vkinit::image_barrier(get_normal_output().image._image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		vkinit::image_barrier(get_uv_output().image._image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		vkinit::image_barrier(get_objID_output().image._image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
	};

	vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, gBufBarriers.size(), gBufBarriers.data());
}

void VulkanGbufferGenerateGraphicsPipeline::barrier_for_gbuffer_generate(VulkanCommandBuffer* cmd)
{
	std::array<VkImageMemoryBarrier, 4> gBufBarriers =
	{
		vkinit::image_barrier(get_wpos_output().image._image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		vkinit::image_barrier(get_normal_output().image._image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		vkinit::image_barrier(get_uv_output().image._image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		vkinit::image_barrier(get_objID_output().image._image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
	};

	vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, 0, 0, 0, gBufBarriers.size(), gBufBarriers.data());
}

const Texture& VulkanGbufferGenerateGraphicsPipeline::get_wpos_output() const
{
	return *_engine->get_engine_texture(ETextureResourceNames::GBUFFER_WPOS);
}

const Texture& VulkanGbufferGenerateGraphicsPipeline::get_normal_output() const
{
	return *_engine->get_engine_texture(ETextureResourceNames::GBUFFER_NORM);
}

const Texture& VulkanGbufferGenerateGraphicsPipeline::get_uv_output() const
{
	return *_engine->get_engine_texture(ETextureResourceNames::GBUFFER_UV);
}

const Texture& VulkanGbufferGenerateGraphicsPipeline::get_objID_output() const
{
	return *_engine->get_engine_texture(ETextureResourceNames::GBUFFER_OBJ_ID);
}

const Texture& VulkanGbufferGenerateGraphicsPipeline::get_depth_output() const
{
	return *_engine->get_engine_texture(ETextureResourceNames::GBUFFER_DEPTH_BUFFER);
}

void VulkanGbufferGenerateGraphicsPipeline::init_gbuffer_tex()
{
	_texExtent = {
		_engine->_windowExtent.width,
		_engine->_windowExtent.height,
		1
	};

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_wposFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, _texExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_wposFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::GBUFFER_WPOS);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_normalFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, _texExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_normalFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::GBUFFER_NORM);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_uvFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, _texExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_uvFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::GBUFFER_UV);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_objIDFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, _texExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_objIDFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::GBUFFER_OBJ_ID);
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		texBuilder.start()
			.make_img_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, _texExtent)
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT)
			.create_engine_texture(ETextureResourceNames::GBUFFER_DEPTH_BUFFER);
	}
}

void VulkanGbufferGenerateGraphicsPipeline::init_render_pass()
{
	RenderPassInfo default_rp;
	default_rp.op_flags = RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
	default_rp.clear_attachments = BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4);
	default_rp.store_attachments = BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4);
	default_rp.num_color_attachments = 4;
	default_rp.color_attachments[0] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_WPOS);
	default_rp.color_attachments[1] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_NORM);
	default_rp.color_attachments[2] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_UV);
	default_rp.color_attachments[3] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_OBJ_ID);
	default_rp.depth_stencil = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_DEPTH_BUFFER);

	RenderPassInfo::Subpass subpass = {};
	subpass.num_color_attachments = 4;
	subpass.depth_stencil_mode = RenderPassInfo::DepthStencil::ReadWrite;
	subpass.color_attachments[0] = 0;
	subpass.color_attachments[1] = 1;
	subpass.color_attachments[2] = 2;
	subpass.color_attachments[3] = 3;

	default_rp.num_subpasses = 1;
	default_rp.subpasses = &subpass;

	_engine->_renderPassManager.get_render_pass(ERenderPassType::GBufferGenerate)->init(_engine, default_rp);

	{
		VkFramebufferCreateInfo fb_info = {};
		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_info.pNext = nullptr;

		fb_info.renderPass = _engine->_renderPassManager.get_render_pass(ERenderPassType::GBufferGenerate)->get_render_pass();
		fb_info.width = _texExtent.width;
		fb_info.height = _texExtent.height;
		fb_info.layers = 1;

		std::array<VkImageView, 5> attachments;
		attachments[0] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_WPOS)->imageView;
		attachments[1] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_NORM)->imageView;
		attachments[2] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_UV)->imageView;
		attachments[3] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_OBJ_ID)->imageView;
		attachments[4] = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_DEPTH_BUFFER)->imageView;

		fb_info.pAttachments = attachments.data();
		fb_info.attachmentCount = attachments.size();
		vkCreateFramebuffer(_engine->_device, &fb_info, nullptr, &_gBufFramebuffer);
	}
}

void VulkanGbufferGenerateGraphicsPipeline::init_scene_buffer(const std::vector<RenderObject>& renderables)
{

	_objectsSize = renderables.size();
	_objectBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanGbufferGenerateGraphicsPipeline::ObjectData) * renderables.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_globalCameraBuffer[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanGbufferGenerateGraphicsPipeline::SGlobalCamera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

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
		VulkanGbufferGenerateGraphicsPipeline::ObjectData* objectSSBO = (VulkanGbufferGenerateGraphicsPipeline::ObjectData*)data;

		for (int i = 0; i < renderables.size(); i++)
		{
			const RenderObject& object = renderables[i];
			objectSSBO[i].model = object.transformMatrix;
			objectSSBO[i].meshIndex = object.meshIndex;
			objectSSBO[i].meshletCount = object.mesh->_meshlets.size();
		}
		});

	_indirectBuffer = _engine->create_cpu_to_gpu_buffer(_objectsSize * sizeof(VkDrawMeshTasksIndirectCommandNV), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

	_engine->map_buffer(_engine->_allocator, _indirectBuffer._allocation, [&](void*& data) {
		VkDrawMeshTasksIndirectCommandNV* drawCommands = (VkDrawMeshTasksIndirectCommandNV*)data;
		//encode the draw data of each object into the indirect draw buffer
		for (int i = 0; i < _engine->_resManager.renderables.size(); i++)
		{
			RenderObject& object = _engine->_resManager.renderables[i];
			drawCommands[i].taskCount = (object.mesh->_meshlets.size() + 31) / 32;
			drawCommands[i].firstTask = 0;
		}
		});

	{
		VkSampler& sampler = _engine->get_engine_sampler(ESamplerType::NEAREST_REPEAT)->sampler;

		VkDescriptorImageInfo wposImageBufferInfo;
		wposImageBufferInfo.sampler = sampler;
		wposImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_WPOS)->imageView;
		wposImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo normImageBufferInfo;
		normImageBufferInfo.sampler = sampler;
		normImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_NORM)->imageView;
		normImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo uvImageBufferInfo;
		uvImageBufferInfo.sampler = sampler;
		uvImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_UV)->imageView;
		uvImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo objImageBufferInfo;
		objImageBufferInfo.sampler = sampler;
		objImageBufferInfo.imageView = _engine->get_engine_texture(ETextureResourceNames::GBUFFER_OBJ_ID)->imageView;
		objImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_image(0, &wposImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL)
			.bind_image(1, &normImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL)
			.bind_image(2, &uvImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL)
			.bind_image(3, &objImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL)
			.build(_engine, EDescriptorResourceNames::GBuffer);
	}
}
#endif
