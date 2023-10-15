#pragma once

#include <vk_types.h>
#include <vk_shaders.h>

class VulkanEngine;

class IVulkanRenderPass
{
public:
	virtual void init(VulkanEngine* engine) = 0;
	virtual void init_render_pass() = 0;
	virtual void init_pipelines() = 0;
	virtual void init_descriptors() = 0;
};

class VulkanMainRenderPass : public IVulkanRenderPass
{
public:
	virtual void init(VulkanEngine* engine) override;
	virtual void init_render_pass() override;
	virtual void init_pipelines() override {};
	virtual void init_descriptors() override {};

	const VkFramebuffer& get_framebuffer(size_t index) const;

	const VkRenderPass& get_render_pass() const;

	const VkSwapchainKHR& get_swap_chain() const;

private:

	void init_swapchain();
	void init_depth_buffer();
	void init_framebuffers();

	VkRenderPass _render_pass;
	VulkanEngine* _engine;

	Texture _depthTex;

	//the format for the depth image
	VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;

	VkSwapchainKHR _swapchain;

	// image format expected by the windowing system
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages = {};
	std::vector<VkImageView> _swapchainImageViews = {};

	std::vector<VkFramebuffer> _framebuffers;
};

class VulkanDepthReduceRenderPass : public IVulkanRenderPass
{
public:
	virtual void init(VulkanEngine* engine) override;
	virtual void init_render_pass() override;
	virtual void init_pipelines() override;
	virtual void init_descriptors() override;

	void create_depth_pyramid(size_t w, size_t h);

private:
	VkRenderPass _render_pass;
	VulkanEngine* _engine;

	//the format for the depth image
	VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;

	VkFormat _swapchainImageFormat; 

	AllocatedImage depthPyramid = {};
	VkImageView depthPyramidMips[16] = {};
	uint32_t depthPyramidLevels = 0;

	ShaderEffect depthreduceEffect;

	VkPipeline _drawcmdPipeline;
	VkPipelineLayout _drawcmdPipelineLayout;

	VkDescriptorSetLayout _objectSetLayout;
	VkDescriptorSet objectDescriptor;

};