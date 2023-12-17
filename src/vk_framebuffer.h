#pragma once

#include <vk_types.h>

class VulkanEngine;
class VulkanRenderPass;
struct RenderPassInfo;

enum class EFramebufferType : uint32_t
{
	NoInit = 0,
	DefaultSwapchain = 1,
	Max,
};

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

	VulkanRenderPass* _render_pass = nullptr;

	VkFramebuffer _framebuffer = VK_NULL_HANDLE;

	uint32_t _width = 0;
	uint32_t _height = 0;
};

class VulkanFrameBufferManager
{
public:
	void init(VulkanEngine* _engine);

	VulkanFrameBuffer* get_framebuffer(EFramebufferType type) const;
private:
	std::vector<std::unique_ptr<VulkanFrameBuffer>> _framebuffersList;
};