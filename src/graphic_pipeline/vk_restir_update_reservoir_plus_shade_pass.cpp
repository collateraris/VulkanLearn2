#include <graphic_pipeline/vk_restir_update_reservoir_plus_shade_pass.h>

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <rhi/vulkan_resources.h>

void VulkanReSTIRUpdateReservoirPlusShadePass::init(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		_engine->_renderExtent.width,
		_engine->_renderExtent.height,
		1
	};

	_tileNumberWidth = _engine->_renderExtent.width / _tileSize + 1;
	_tileNumberHeight = _engine->_renderExtent.height / _tileSize + 1;

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_outputTex = texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}

	// Keep the raw total untouched; NRD receives separate linear radiance and
	// unnormalized first-hit distances through these additional outputs.
	auto makeDenoiserOutput = [&]() {
		const auto image = engine->_rhi.resources().create_image({
			_imageExtent.width, _imageExtent.height, rhi::Format::Rgba32Float,
			rhi::ImageUsage::Sampled | rhi::ImageUsage::Storage | rhi::ImageUsage::TransferSource});
		engine->_mainDeletionQueue.push_function([engine, image]() { engine->_rhi.resources().destroy(image); });
		return engine->_rhi.vulkan_resources().texture(image);
	};
	_diffuseOutput = makeDenoiserOutput();
	_specularOutput = makeDenoiserOutput();
	_bypassOutput = makeDenoiserOutput();
	init_description_set_global_buffer();

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::ReSTIR_UpdateReservoir_PlusShade,
			[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect computeEffect;
				computeEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("restirShade.comp.slang.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
				computeEffect.reflect_layout(engine->_device, nullptr, 0);

				ComputePipelineBuilder computePipelineBuilder;
				computePipelineBuilder.setShaders(&computeEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->setLayout,
																	_engine->get_engine_descriptor(EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0)->setLayout,
																	_rpDescrMan.get_layout() };
				mesh_pipeline_layout_info.setLayoutCount = setLayout.size();
				mesh_pipeline_layout_info.pSetLayouts = setLayout.data();
				VkPushConstantRange denoiserPush{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t)};
				mesh_pipeline_layout_info.pushConstantRangeCount = 1;
				mesh_pipeline_layout_info.pPushConstantRanges = &denoiserPush;

				vkCreatePipelineLayout(_engine->_device, &mesh_pipeline_layout_info, nullptr, &computePipelineBuilder._pipelineLayout);
				//hook the push constants layout
				pipelineLayout = computePipelineBuilder._pipelineLayout;

				pipeline = computePipelineBuilder.build_compute_pipeline(engine->_device);
			});
	}
}

void VulkanReSTIRUpdateReservoirPlusShadePass::init_description_set_global_buffer()
{
	_rpDescrMan = vkutil::DescriptorManagerBuilder::begin(_engine, _engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_image(0, ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(1, ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(2, ETextureResourceNames::PT_GBUFFER_NORMAL, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(3, ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(4, _outputTex, EResOp::WRITE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(5, _diffuseOutput, EResOp::WRITE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(6, _specularOutput, EResOp::WRITE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(7, _bypassOutput, EResOp::WRITE, VK_SHADER_STAGE_COMPUTE_BIT)
		.create_desciptor_manager();
}

void VulkanReSTIRUpdateReservoirPlusShadePass::draw(rhi::CommandList& cmd, int frameSlot)
{
	const auto pipeline = _engine->_rhi.pipeline(
		_engine->_renderPipelineManager.get_pipeline(EPipelineType::ReSTIR_UpdateReservoir_PlusShade),
		_engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_UpdateReservoir_PlusShade),
		VK_PIPELINE_BIND_POINT_COMPUTE);
	cmd.bind_pipeline(pipeline);
	cmd.bind_descriptor_set(pipeline, 0, _engine->_rhi.descriptor(
		_engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->set));

	const EDescriptorResourceNames currentGlobalUniformsDesc = frameSlot % 2 == 0
		? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0
		: EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;
	cmd.bind_descriptor_set(pipeline, 1, _engine->_rhi.descriptor(
		_engine->get_engine_descriptor(currentGlobalUniformsDesc)->set));

	cmd.bind_descriptor_set(pipeline, 2, _engine->_rhi.descriptor(_rpDescrMan.get_set()));
	const uint32_t denoiserEnabled = _engine->_denoiserEnabled ? 1u : 0u;
	cmd.push_constants(pipeline, rhi::Stage::Compute, &denoiserEnabled, sizeof(denoiserEnabled));

	cmd.dispatch(_tileNumberWidth, _tileNumberHeight, 1);
}

const Texture& VulkanReSTIRUpdateReservoirPlusShadePass::get_output() const
{
	return _outputTex;
}
