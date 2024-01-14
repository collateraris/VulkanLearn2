#pragma once 

#include <vk_types.h>

class ShaderCache;
class VulkanRenderPassManager;
class VulkanEngine;

class VulkanRenderPipeline
{
public:
	VulkanRenderPipeline() = default;
	~VulkanRenderPipeline();

	void init(VulkanEngine* engine, EPipelineType type, std::function<void(VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)>&& func);

	EPipelineType get_type() const;
	VkPipeline get_pipeline() const;
	VkPipelineLayout get_pipelineLayout() const;

private:
	VulkanEngine* _engine = nullptr;
	EPipelineType _type = EPipelineType::NoInit;
	VkPipeline _pipeline = VK_NULL_HANDLE;
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
};

class VulkanRenderPipelineManager
{
public:
	void init(VulkanEngine* engine, VulkanRenderPassManager* renderPassManager, ShaderCache* shaderCache);

	void init_render_pipeline(VulkanEngine* engine, EPipelineType type, std::function<void(VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)>&& func);
	VkPipeline get_pipeline(EPipelineType type) const;
	VkPipelineLayout get_pipelineLayout(EPipelineType type) const;

private:
	std::vector<VulkanRenderPipeline> _pipelinesList;
};