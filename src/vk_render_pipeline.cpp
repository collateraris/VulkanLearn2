#include "vk_render_pipeline.h"

#include <vk_material_system.h>
#include <vk_render_pass.h>
#include <vk_shaders.h>
#include <vk_engine.h>
#include <vk_initializers.h>

VulkanRenderPipeline::~VulkanRenderPipeline()
{
	vkDestroyPipeline(_engine->_device, _pipeline, nullptr);

	vkDestroyPipelineLayout(_engine->_device, _pipelineLayout, nullptr);
}

void VulkanRenderPipeline::init(VulkanEngine* engine, EPipelineType type, std::function<void(VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)>&& func)
{
	_engine = engine;
	_type = type;

	func(_pipeline, _pipelineLayout);
}

EPipelineType VulkanRenderPipeline::get_type() const
{
	return _type;
}

VkPipeline VulkanRenderPipeline::get_pipeline() const
{
	return _pipeline;
}

VkPipelineLayout VulkanRenderPipeline::get_pipelineLayout() const
{
	return _pipelineLayout;
}

void VulkanRenderPipelineManager::init(VulkanEngine* engine, VulkanRenderPassManager* renderPassManager, ShaderCache* shaderCache)
{
	_pipelinesList.clear();
	_pipelinesList.resize(static_cast<uint32_t>(EPipelineType::Max));

	//EPipelineType::PyramidDepthReduce
	{
		_pipelinesList[static_cast<uint32_t>(EPipelineType::PyramidDepthReduce)]
			.init(engine, EPipelineType::PyramidDepthReduce,
				[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
					ShaderEffect computeEffect;
					computeEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("depthreduce.comp.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
					computeEffect.reflect_layout(engine->_device, nullptr, 0);

					ComputePipelineBuilder computePipelineBuilder;
					computePipelineBuilder.setShaders(&computeEffect);
					//hook the push constants layout
					pipelineLayout = computePipelineBuilder._pipelineLayout;

					pipeline = computePipelineBuilder.build_compute_pipeline(engine->_device);
				});
	}

	//EPipelineType::ComputePrepassForTaskMeshIndirect
	{
		_pipelinesList[static_cast<uint32_t>(EPipelineType::ComputePrepassForTaskMeshIndirect)]
			.init(engine, EPipelineType::ComputePrepassForTaskMeshIndirect,
				[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
					ShaderEffect computeEffect;
					computeEffect.add_stage(shaderCache->get_shader(VulkanEngine::shader_path("drawcmd.comp.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
					computeEffect.reflect_layout(engine->_device, nullptr, 0);

					ComputePipelineBuilder computePipelineBuilder;
					computePipelineBuilder.setShaders(&computeEffect);
					//hook the push constants layout
					pipelineLayout = computePipelineBuilder._pipelineLayout;

					pipeline = computePipelineBuilder.build_compute_pipeline(engine->_device);
				});
	}
}

void VulkanRenderPipelineManager::init_render_pipeline(VulkanEngine* engine, EPipelineType type, std::function<void(VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)>&& func)
{
	_pipelinesList[static_cast<uint32_t>(type)]
		.init(engine, type, std::move(func));
}

VkPipeline VulkanRenderPipelineManager::get_pipeline(EPipelineType type) const
{
	return _pipelinesList[static_cast<uint32_t>(type)].get_pipeline();
}

VkPipelineLayout VulkanRenderPipelineManager::get_pipelineLayout(EPipelineType type) const
{
	return _pipelinesList[static_cast<uint32_t>(type)].get_pipelineLayout();
}
