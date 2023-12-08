#include <vk_render_pass.h>

#include <vk_utils.h>
#include <vk_engine.h>

void VulkanRenderPass::init(VulkanEngine* engine, const VkRenderPassCreateInfo& create_info)
{
	setup_subpasses(create_info);

	engine->create_render_pass(create_info, render_pass);
}

void VulkanRenderPass::setup_subpasses(const VkRenderPassCreateInfo& create_info)
{
	auto* attachments = create_info.pAttachments;

	// Store the important subpass information for later.
	for (uint32_t i = 0; i < create_info.subpassCount; i++)
	{
		auto& subpass = create_info.pSubpasses[i];

		SubpassInfo subpass_info = {};
		subpass_info.num_color_attachments = subpass.colorAttachmentCount;
		subpass_info.num_input_attachments = subpass.inputAttachmentCount;
		subpass_info.depth_stencil_attachment = *subpass.pDepthStencilAttachment;
		memcpy(subpass_info.color_attachments, subpass.pColorAttachments,
			subpass.colorAttachmentCount * sizeof(*subpass.pColorAttachments));
		memcpy(subpass_info.input_attachments, subpass.pInputAttachments,
			subpass.inputAttachmentCount * sizeof(*subpass.pInputAttachments));

		uint32_t samples = 0;
		for (uint32_t att = 0; att < subpass_info.num_color_attachments; att++)
		{
			if (subpass_info.color_attachments[att].attachment == VK_ATTACHMENT_UNUSED)
				continue;

			uint32_t samp = attachments[subpass_info.color_attachments[att].attachment].samples;
			if (samples && (samp != samples))
				VK_ASSERT(samp == samples);
			samples = samp;
		}

		if (subpass_info.depth_stencil_attachment.attachment != VK_ATTACHMENT_UNUSED)
		{
			uint32_t samp = attachments[subpass_info.depth_stencil_attachment.attachment].samples;
			if (samples && (samp != samples))
				VK_ASSERT(samp == samples);
			samples = samp;
		}

		VK_ASSERT(samples > 0);
		subpass_info.samples = samples;
		subpasses_info.push_back(subpass_info);
	}
}

uint32_t VulkanRenderPass::get_num_subpasses() const
{
	return uint32_t(subpasses_info.size());
}

VkRenderPass VulkanRenderPass::get_render_pass() const
{
	return render_pass;
}

uint32_t VulkanRenderPass::get_sample_count(uint32_t subpass) const
{
	VK_ASSERT(subpass < subpasses_info.size());
	return subpasses_info[subpass].samples;
}

uint32_t VulkanRenderPass::get_num_color_attachments(uint32_t subpass) const
{
	VK_ASSERT(subpass < subpasses_info.size());
	return subpasses_info[subpass].num_color_attachments;
}

uint32_t VulkanRenderPass::get_num_input_attachments(uint32_t subpass) const
{
	VK_ASSERT(subpass < subpasses_info.size());
	return subpasses_info[subpass].num_input_attachments;
}

const VkAttachmentReference& VulkanRenderPass::get_color_attachment(uint32_t subpass, uint32_t index) const
{
	VK_ASSERT(subpass < subpasses_info.size());
	VK_ASSERT(index < subpasses_info[subpass].num_color_attachments);
	return subpasses_info[subpass].color_attachments[index];
}

const VkAttachmentReference& VulkanRenderPass::get_input_attachment(uint32_t subpass, uint32_t index) const
{
	VK_ASSERT(subpass < subpasses_info.size());
	VK_ASSERT(index < subpasses_info[subpass].num_input_attachments);
	return subpasses_info[subpass].input_attachments[index];
}

bool VulkanRenderPass::has_depth(uint32_t subpass) const
{
	VK_ASSERT(subpass < subpasses_info.size());
	return subpasses_info[subpass].depth_stencil_attachment.attachment != VK_ATTACHMENT_UNUSED &&
		vkutil::format_has_depth_aspect(depth_stencil);
}

bool VulkanRenderPass::has_stencil(uint32_t subpass) const
{
	VK_ASSERT(subpass < subpasses_info.size());
	return subpasses_info[subpass].depth_stencil_attachment.attachment != VK_ATTACHMENT_UNUSED &&
		vkutil::format_has_stencil_aspect(depth_stencil);
}
