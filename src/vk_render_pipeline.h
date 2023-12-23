#pragma once 

#include <vk_types.h>

class ShaderCache;
class VulkanRenderPassManager;
class VulkanEngine;

enum class EPipelineType : uint32_t
{
	NoInit = 0,
	Bindless_TaskMeshIndirectForward = 1,
	DrawIndirectForward = 2,
	PyramidDepthReduce = 3,
	ComputePrepassForTaskMeshIndirect = 4,
	BaseRaytracer = 5,
	FullScreen = 6,
	VbufferGenerate = 7,
	Max,
};

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