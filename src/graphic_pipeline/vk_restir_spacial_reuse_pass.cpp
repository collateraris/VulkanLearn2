#include <graphic_pipeline/vk_restir_spacial_reuse_pass.h>

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>

void VulkanReSTIRSpaceReusePass::init(VulkanEngine* engine)
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
		init_description_set_global_buffer();
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::ReSTIR_DI_SpaceReuse,
			[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect computeEffect;
				computeEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("restirDISpacial.comp.slang.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
				computeEffect.reflect_layout(engine->_device, nullptr, 0);

				ComputePipelineBuilder computePipelineBuilder;
				computePipelineBuilder.setShaders(&computeEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->setLayout,
																	_engine->get_engine_descriptor(EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0)->setLayout,
																	_rpDescrMan.get_layout() };
				mesh_pipeline_layout_info.setLayoutCount = setLayout.size();
				mesh_pipeline_layout_info.pSetLayouts = setLayout.data();

				vkCreatePipelineLayout(_engine->_device, &mesh_pipeline_layout_info, nullptr, &computePipelineBuilder._pipelineLayout);
				//hook the push constants layout
				pipelineLayout = computePipelineBuilder._pipelineLayout;

				pipeline = computePipelineBuilder.build_compute_pipeline(engine->_device);
			});
	}
}

Texture& VulkanReSTIRSpaceReusePass::get_tex(ETextureResourceNames name) const
{
	return *_engine->get_engine_texture(name);
}

void VulkanReSTIRSpaceReusePass::init_description_set_global_buffer()
{
	_rpDescrMan = vkutil::DescriptorManagerBuilder::begin(_engine, _engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_image(0, ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(1, ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(2, ETextureResourceNames::PT_GBUFFER_NORMAL, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(3, ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.create_desciptor_manager();
}
void VulkanReSTIRSpaceReusePass::draw(rhi::CommandList& cmd, int frameSlot)
{
	const auto pipeline = _engine->_rhi.pipeline(
		_engine->_renderPipelineManager.get_pipeline(EPipelineType::ReSTIR_DI_SpaceReuse),
		_engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::ReSTIR_DI_SpaceReuse),
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

	cmd.dispatch(_tileNumberWidth, _tileNumberHeight, 1);
}
