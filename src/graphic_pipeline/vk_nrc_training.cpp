#include <graphic_pipeline/vk_nrc_training.h>

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <vk_utils.h>

void VulkanNRC_TrainingPass::init(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		_engine->_windowExtent.width,
		_engine->_windowExtent.height,
		1
	};

	_tileNumberWidth = _engine->_windowExtent.width / _tileSize + 1;
	_tileNumberHeight = _engine->_windowExtent.height / _tileSize + 1;

	{
		init_description_set_global_buffer();
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::NRC_Training,
			[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect computeEffect;
				computeEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("nrc_training.comp.slang.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
				computeEffect.reflect_layout(engine->_device, nullptr, 0);

				ComputePipelineBuilder computePipelineBuilder;
				computePipelineBuilder.setShaders(&computeEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->setLayout,
																	_engine->get_engine_descriptor(EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0)->setLayout,
																	_engine->get_engine_descriptor(EDescriptorResourceNames::NRC_GlobalUniformBuffer_Frame0)->setLayout,
																	_engine->get_engine_descriptor(EDescriptorResourceNames::NRC_MLP_Train_Inference)->setLayout,
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

void VulkanNRC_TrainingPass::init_description_set_global_buffer()
{
	_rpDescrMan = vkutil::DescriptorManagerBuilder::begin(_engine, _engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_image(0, ETextureResourceNames::PT_GBUFFER_ALBEDO_METALNESS, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(1, ETextureResourceNames::PT_GBUFFER_EMISSION_ROUGHNESS, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(2, ETextureResourceNames::PT_GBUFFER_NORMAL, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(3, ETextureResourceNames::PT_GBUFFER_WPOS_OBJECT_ID, EResOp::READ_STORAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.create_desciptor_manager();
}

Texture& VulkanNRC_TrainingPass::get_tex(ETextureResourceNames name) const
{
	return *_engine->get_engine_texture(name);
}

void VulkanNRC_TrainingPass::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	const NeuralRadianceCache& nrc = *_engine->_resManager.nrc_cache.get();
	cmd->dispatch(_tileNumberWidth, _tileNumberHeight, 1, [&](VkCommandBuffer cmd)
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipeline(EPipelineType::NRC_Training));

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::NRC_Training), 0,
			1, &_engine->get_engine_descriptor(EDescriptorResourceNames::Bindless_Scene)->set, 0, nullptr);

		EDescriptorResourceNames currentGlobalUniformsDesc = current_frame_index % 2 == 0
			? EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame0
			: EDescriptorResourceNames::GI_GlobalUniformBuffer_Frame1;

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::NRC_Training), 1,
			1, &_engine->get_engine_descriptor(currentGlobalUniformsDesc)->set, 0, nullptr);

		EDescriptorResourceNames currentNRCUniformsDesc = current_frame_index % 2 == 0
			? EDescriptorResourceNames::NRC_GlobalUniformBuffer_Frame0
			: EDescriptorResourceNames::NRC_GlobalUniformBuffer_Frame1;

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::NRC_Training), 2,
			1, &_engine->get_engine_descriptor(currentNRCUniformsDesc)->set, 0, nullptr);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::NRC_Training), 3,
			1, &_engine->get_engine_descriptor(EDescriptorResourceNames::NRC_MLP_Train_Inference)->set, 0, nullptr);

		_rpDescrMan.bind_descriptor_set(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::NRC_Inference), 4);
	});
}
