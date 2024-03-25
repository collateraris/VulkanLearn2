#include "vk_simple_accumulation_graphics_pipeline.h"

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

void VulkanSimpleAccumulationGraphicsPipeline::init(VulkanEngine* engine, const Texture& currentTex)
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
		_outputTexture = texBuilder.start()
			.make_img_info(_outputFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |VK_IMAGE_USAGE_TRANSFER_SRC_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_outputFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_lastFrameTexture = texBuilder.start()
			.make_img_info(_lastFrameFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_lastFrameFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}

	init_render_pass();
	init_description_set(currentTex);

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::SimpleAccumulation,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect defaultEffect;
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("fullscreen.vert.spv")), VK_SHADER_STAGE_VERTEX_BIT);
				defaultEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_path("accumulate.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

				defaultEffect.reflect_layout(engine->_device, nullptr, 0);
				//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
				GraphicPipelineBuilder pipelineBuilder;

				pipelineBuilder.setShaders(&defaultEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _globalDescSetLayout, _imageDescSetLayout };
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
				pipelineBuilder._viewport.width = (float)engine->_windowExtent.width;
				pipelineBuilder._viewport.height = (float)engine->_windowExtent.height;
				pipelineBuilder._viewport.minDepth = 0.0f;
				pipelineBuilder._viewport.maxDepth = 1.0f;

				pipelineBuilder._scissor.offset = { 0, 0 };
				pipelineBuilder._scissor.extent = engine->_windowExtent;

				//configure the rasterizer to draw filled triangles
				pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
				pipelineBuilder._rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
				pipelineBuilder._rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

				//we dont use multisampling, so just run the default one
				pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

				//a single blend attachment with no blending and writing to RGBA
				pipelineBuilder.attachment_count = 1;
				pipelineBuilder._colorBlendAttachment.push_back(vkinit::color_blend_attachment_state());

				//default depthtesting
				pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(false, false , VK_COMPARE_OP_LESS_OR_EQUAL);

				pipelineBuilder.vertexDescription = Vertex::get_vertex_description();
				pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
				//connect the pipeline builder vertex input info to the one we get from Vertex
				pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = nullptr;
				pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = 0;

				pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = nullptr;
				pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = 0;

				VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(engine->_device,
					_engine->_renderPassManager.get_render_pass(ERenderPassType::SimpleAccumulation)->get_render_pass());

				pipeline = meshPipeline;
				pipelineLayout = meshPipLayout;
			});
	}
	
}

Texture& VulkanSimpleAccumulationGraphicsPipeline::get_tex(ETextureResourceNames name) const
{
	return *_engine->get_engine_texture(name);
}

void VulkanSimpleAccumulationGraphicsPipeline::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	if (_counter.accumCount == 0)
	{
			VkClearValue clear_value = { 0., 0., 0., 1. };

			cmd->clear_image(_lastFrameTexture, clear_value);
			cmd->clear_image(get_tex(ETextureResourceNames::ReSTIR_DI_PREV_RESERVOIRS), clear_value);
			cmd->clear_image(get_tex(ETextureResourceNames::ReSTIR_GI_PREV_RESERVOIRS), clear_value);
			cmd->clear_image(get_tex(ETextureResourceNames::ReSTIR_INDIRECT_LO_PREV), clear_value);
			cmd->clear_image(get_tex(ETextureResourceNames::ReSTIR_GI_SAMPLES_POSITION_PREV), clear_value);
			cmd->clear_image(get_tex(ETextureResourceNames::ReSTIR_GI_SAMPLES_NORMAL_PREV), clear_value);
	}


	_engine->map_buffer(_engine->_allocator, _perFrameCount[current_frame_index]._allocation, [&](void*& data) {
		memcpy(data, &_counter, sizeof(VulkanSimpleAccumulationGraphicsPipeline::PerFrameCB));
		});

	//make a clear-color from frame number. This will flash with a 120*pi frame period.
	VkClearValue clearValue;
	clearValue.color = { { 1.0f, 1.0f, 1.f, 1.0f } };

	//start the main renderpass. 
	//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_engine->_renderPassManager.get_render_pass(ERenderPassType::SimpleAccumulation)->get_render_pass(), VkExtent2D(_imageExtent.width, _imageExtent.height), _simpleAccumFramebuffer);

	//connect clear values
	rpInfo.clearValueCount = 1;

	VkClearValue clearValues[] = { clearValue};

	rpInfo.pClearValues = &clearValues[0];

	vkCmdBeginRenderPass(cmd->get_cmd(), &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)_imageExtent.width;
	viewport.height = (float)_imageExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset = { 0, 0 };
	scissor.extent = VkExtent2D(_imageExtent.width, _imageExtent.height);

	vkCmdSetViewport(cmd->get_cmd(), 0, 1, &viewport);
	vkCmdSetScissor(cmd->get_cmd(), 0, 1, &scissor);
	vkCmdSetDepthBias(cmd->get_cmd(), 0, 0, 0);

	cmd->draw_quad([&](VkCommandBuffer cmd) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipeline(EPipelineType::SimpleAccumulation));
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::SimpleAccumulation), 0,
			1, &_globalDescSet[current_frame_index], 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::SimpleAccumulation), 1,
			1, &_imageDescSet[current_frame_index], 0, nullptr);
		});

	//finalize the render pass
	vkCmdEndRenderPass(cmd->get_cmd());

	{
		std::array<VkImageMemoryBarrier, 1> outputBarriers =
		{
			vkinit::image_barrier(_outputTexture.image._image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, outputBarriers.size(), outputBarriers.data());

		std::array<VkImageMemoryBarrier, 1> lastFrameBarriers =
		{
			vkinit::image_barrier(_lastFrameTexture.image._image,  VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, lastFrameBarriers.size(), lastFrameBarriers.data());
	}

	cmd->blit_image(_lastFrameTexture, _outputTexture, 
		{ 0, 0, 0 }, { (int)_imageExtent.width, (int)_imageExtent.height , 1 },
		{ 0, 0, 0 }, { (int)_imageExtent.width, (int)_imageExtent.height , 1 }, 0, 0);

	{
		std::array<VkImageMemoryBarrier, 1> outputBarriers =
		{
			vkinit::image_barrier(_outputTexture.image._image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  0, 0, 0, 0, 0, outputBarriers.size(), outputBarriers.data());

		std::array<VkImageMemoryBarrier, 1> lastFrameBarriers =
		{
			vkinit::image_barrier(_lastFrameTexture.image._image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT,  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd->get_cmd(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, lastFrameBarriers.size(), lastFrameBarriers.data());
	}

	_counter.accumCount++;
	_counter.initLastFrame = 1;
}

void VulkanSimpleAccumulationGraphicsPipeline::try_reset_accumulation(PlayerCamera& camera)
{
	auto view = camera.get_view_matrix();
	if (_lastViewMatrix != view)
	{
		_lastViewMatrix = view;
		_counter.accumCount = 0;
	}
}

void VulkanSimpleAccumulationGraphicsPipeline::reset_accumulation()
{
	_counter.accumCount = 0;
}

const Texture& VulkanSimpleAccumulationGraphicsPipeline::get_output() const
{
    return _lastFrameTexture;
}

void VulkanSimpleAccumulationGraphicsPipeline::init_render_pass()
{
	RenderPassInfo default_rp;
	default_rp.op_flags = RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
	default_rp.clear_attachments = BIT(0);
	default_rp.store_attachments = BIT(0);
	default_rp.num_color_attachments = 1;
	default_rp.color_attachments[0] = &_outputTexture;
	default_rp.depth_stencil = nullptr;

	RenderPassInfo::Subpass subpass = {};
	subpass.num_color_attachments = 1;
	subpass.depth_stencil_mode = RenderPassInfo::DepthStencil::None;
	subpass.color_attachments[0] = 0;

	default_rp.num_subpasses = 1;
	default_rp.subpasses = &subpass;

	_engine->_renderPassManager.get_render_pass(ERenderPassType::SimpleAccumulation)->init(_engine, default_rp);

	{
		VkFramebufferCreateInfo fb_info = {};
		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_info.pNext = nullptr;

		fb_info.renderPass = _engine->_renderPassManager.get_render_pass(ERenderPassType::SimpleAccumulation)->get_render_pass();
		fb_info.width = _imageExtent.width;
		fb_info.height = _imageExtent.height;
		fb_info.layers = 1;

		VkImageView attachments[1];
		attachments[0] = _outputTexture.imageView;

		fb_info.pAttachments = attachments;
		fb_info.attachmentCount = 1;
		vkCreateFramebuffer(_engine->_device, &fb_info, nullptr, &_simpleAccumFramebuffer);
	}
}

void VulkanSimpleAccumulationGraphicsPipeline::init_description_set(const Texture& currentTex)
{
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSampler sampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &sampler);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VkDescriptorImageInfo currentFrameImageBufferInfo;
		currentFrameImageBufferInfo.sampler = sampler;
		currentFrameImageBufferInfo.imageView = currentTex.imageView;
		currentFrameImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo prevImageBufferInfo;
		prevImageBufferInfo.sampler = sampler;
		prevImageBufferInfo.imageView = _lastFrameTexture.imageView;
		prevImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		
		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_image(0, &currentFrameImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.bind_image(1, &prevImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(_imageDescSet[i], _imageDescSetLayout);

		_perFrameCount[i] = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanSimpleAccumulationGraphicsPipeline::PerFrameCB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		VkDescriptorBufferInfo globalUniformsInfo;
		globalUniformsInfo.buffer = _perFrameCount[i]._buffer;
		globalUniformsInfo.offset = 0;
		globalUniformsInfo.range = _perFrameCount[i]._size;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(_globalDescSet[i], _globalDescSetLayout);
	}
}

#endif
