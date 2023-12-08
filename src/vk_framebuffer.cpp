#include "vk_framebuffer.h"

#include <vk_engine.h>
#include <vk_render_pass.h>

void VulkanFrameBuffer::init(VulkanEngine* engine, VulkanRenderPass* render_pass, const RenderPassInfo& create_info)
{
	_render_pass = render_pass;

	RenderPassInfo::compute_dimensions(create_info, _width, _height);
	std::vector<VkImageView> views = RenderPassInfo::setup_raw_views(create_info);
	uint32_t num_views = views.size();

	VkFramebufferCreateInfo fb_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	fb_info.renderPass = _render_pass->get_render_pass();
	fb_info.attachmentCount = num_views;
	fb_info.pAttachments = views.data();
	fb_info.width = _width;
	fb_info.height = _height;
	fb_info.layers = 1; // For multiview, layers must be 1. The render pass encodes a mask.

	vkCreateFramebuffer(engine->_device, &fb_info, nullptr, &_framebuffer);
}
