#pragma once

#include <vk_types.h>

class VulkanEngine;
class VulkanRenderPass;
struct RenderPassInfo;

class VulkanFrameBuffer
{
public:
	VulkanFrameBuffer() = default;

	void init(VulkanEngine* engine, VulkanRenderPass* render_pass, const RenderPassInfo& create_info);

	VkFramebuffer get_framebuffer() const
	{
		return _framebuffer;
	}

	uint32_t get_width() const
	{
		return _width;
	}

	uint32_t get_height() const
	{
		return _height;
	}

	const VulkanRenderPass& get_compatible_render_pass() const
	{
		return *_render_pass;
	}

private:

	VulkanRenderPass* _render_pass;

	VkFramebuffer _framebuffer = VK_NULL_HANDLE;

	uint32_t _width = 0;
	uint32_t _height = 0;
};