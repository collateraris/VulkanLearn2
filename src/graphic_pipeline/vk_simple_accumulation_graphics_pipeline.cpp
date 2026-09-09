#include "vk_simple_accumulation_graphics_pipeline.h"

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_render_graph.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <vk_camera.h>
#include <rhi/vulkan_resources.h>
#include <cstdlib>
#include <cstring>

void VulkanSimpleAccumulationGraphicsPipeline::init(VulkanEngine* engine, const Texture& currentTex)
{
    _engine = engine;
	_sourceTexture = &currentTex;
	if (const char* setting = std::getenv("RESTIR_ACCUMULATION"))
		_engine->_frameAccumulationEnabled = std::strcmp(setting, "0") != 0;
	_accumulationEnabled = _engine->_frameAccumulationEnabled;
	reset_accumulation();

	_imageExtent = {
	_engine->_windowExtent.width,
	_engine->_windowExtent.height,
	1
	};

	const auto makeImage = [&](rhi::ImageUsage usage) {
		const auto image = _engine->_rhi.resources().create_image({
			_imageExtent.width, _imageExtent.height, rhi::Format::Rgba32Float, usage});
		_engine->_mainDeletionQueue.push_function([engine = _engine, image]() {
			engine->_rhi.resources().destroy(image);
		});
		// Existing framebuffer/descriptor setup consumes an explicit native view.
		return _engine->_rhi.vulkan_resources().texture(image);
	};
	_outputTexture = makeImage(rhi::ImageUsage::ColorAttachment | rhi::ImageUsage::TransferSource);
	_lastFrameTexture = makeImage(rhi::ImageUsage::Sampled | rhi::ImageUsage::TransferDestination | rhi::ImageUsage::TransferSource);

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

void VulkanSimpleAccumulationGraphicsPipeline::append_passes(rg::RenderGraph& graph, int frameSlot)
{
	const bool accumulationEnabled = _engine->_frameAccumulationEnabled;
	if (!accumulationEnabled || accumulationEnabled != _accumulationEnabled)
		reset_accumulation();
	_accumulationEnabled = accumulationEnabled;

	// The frame fence has completed before registration. Both passes use this
	// immutable counter snapshot; recording the copy commits the next count.
	const PerFrameCB counter = _counter;
	_engine->_rhi.resources().write_buffer(_engine->_rhi.buffer(_perFrameCount[frameSlot]), &counter, sizeof(counter));

	auto& device = _engine->_rhi;
	const auto sourceImage = device.image(*_sourceTexture);
	const auto scratchImage = device.image(_outputTexture);
	const auto historyImage = device.image(_lastFrameTexture);
	const auto source = graph.import_resource("Accumulation.Source", sourceImage, false);
	const auto scratch = graph.import_resource("Accumulation.Scratch", scratchImage, false);
	// On the first frame the shader skips this read, but its descriptor still
	// needs a valid sampled layout. Subsequent frames read before overwriting.
	const auto history = graph.import_resource("Accumulation.History", historyImage);
	const auto uniforms = graph.import_resource("Accumulation.Uniforms", device.buffer(_perFrameCount[frameSlot]));
	const auto pipeline = device.pipeline(_engine->_renderPipelineManager.get_pipeline(EPipelineType::SimpleAccumulation),
		_engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::SimpleAccumulation), VK_PIPELINE_BIND_POINT_GRAPHICS);
	const auto target = device.render_target(
		_engine->_renderPassManager.get_render_pass(ERenderPassType::SimpleAccumulation)->get_render_pass(),
		_simpleAccumFramebuffer, _imageExtent.width, _imageExtent.height);
	const auto uniformSet = device.descriptor(_globalDescSet[frameSlot]);
	const auto imageSet = device.descriptor(_imageDescSet[frameSlot]);
	using rhi::Stage;
	using rhi::Access;
	using rhi::Layout;
	graph.add_pass("Accumulation.Mean", {
		{source, {Stage::Fragment, Access::ShaderRead, Layout::ShaderReadOnly}},
		{history, {Stage::Fragment, Access::ShaderRead, Layout::ShaderReadOnly}},
		{uniforms, {Stage::Fragment, Access::UniformRead, Layout::Undefined}},
		{scratch, {Stage::ColorOutput, Access::ColorWrite, Layout::ColorAttachment}}
	}, [pipeline, target, uniformSet, imageSet](rhi::CommandList& cmd) {
		rhi::ClearValues clear;
		clear.color[0] = clear.color[1] = clear.color[2] = 1.f;
		cmd.begin_render_pass(target, clear);
		cmd.bind_pipeline(pipeline);
		cmd.bind_descriptor_set(pipeline, 0, uniformSet);
		cmd.bind_descriptor_set(pipeline, 1, imageSet);
		cmd.draw(3);
		cmd.end_render_pass();
	});
	const uint32_t width = _imageExtent.width;
	const uint32_t height = _imageExtent.height;
	graph.add_pass("Accumulation.StoreHistory", {
		{scratch, {Stage::Transfer, Access::TransferRead, Layout::TransferSource}},
		{history, {Stage::Transfer, Access::TransferWrite, Layout::TransferDestination}}
	}, [this, scratchImage, historyImage, width, height, counter](rhi::CommandList& cmd) {
		// Identical FP32 formats and extents preserve the exact running mean.
		cmd.copy_image(scratchImage, historyImage, width, height);
		_counter = counter;
		if (_counter.accumCount < (1u << 24) - 1u)
			_counter.accumCount++;
		_counter.initLastFrame = 1;
	});
}

void VulkanSimpleAccumulationGraphicsPipeline::try_reset_accumulation(PlayerCamera& camera)
{
	const auto view = camera.get_view_matrix();
	const auto projection = camera.get_projection_matrix(false);
	if (_lastViewMatrix != view || _lastProjectionMatrix != projection)
	{
		_lastViewMatrix = view;
		_lastProjectionMatrix = projection;
		reset_accumulation();
	}
}

void VulkanSimpleAccumulationGraphicsPipeline::reset_accumulation()
{
	_counter = {};
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
		_engine->_mainDeletionQueue.push_function([device = _engine->_device, framebuffer = _simpleAccumFramebuffer]() {
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		});
	}
}

void VulkanSimpleAccumulationGraphicsPipeline::init_description_set(const Texture& currentTex)
{
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSampler sampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &sampler);
	_engine->_mainDeletionQueue.push_function([device = _engine->_device, sampler]() {
		vkDestroySampler(device, sampler, nullptr);
	});

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

		const auto buffer = _engine->_rhi.resources().create_buffer({
			sizeof(PerFrameCB), rhi::BufferUsage::Uniform, rhi::MemoryUsage::Upload});
		_perFrameCount[i] = _engine->_rhi.vulkan_resources().buffer(buffer);
		_engine->_mainDeletionQueue.push_function([engine = _engine, buffer]() {
			engine->_rhi.resources().destroy(buffer);
		});

		VkDescriptorBufferInfo globalUniformsInfo;
		globalUniformsInfo.buffer = _perFrameCount[i]._buffer;
		globalUniformsInfo.offset = 0;
		globalUniformsInfo.range = VK_WHOLE_SIZE;

		vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
			.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(_globalDescSet[i], _globalDescSetLayout);
	}
}
