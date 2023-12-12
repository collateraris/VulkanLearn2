#include <vk_render_pass.h>

#include <vk_utils.h>
#include <vk_engine.h>

void VulkanRenderPass::init(VulkanEngine* engine, const VkRenderPassCreateInfo& create_info_)
{
	create_info = create_info_;

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

const VkRenderPassCreateInfo& VulkanRenderPass::get_create_info() const
{
	return create_info;
}

void RenderPassInfo::compute_dimensions(const RenderPassInfo& info, uint32_t& width, uint32_t& height)
{
	width = UINT32_MAX;
	height = UINT32_MAX;

	for (unsigned i = 0; i < info.num_color_attachments; i++)
	{
		VK_ASSERT(info.color_attachments[i]);
		width = std::min(width, info.color_attachments[i]->extend.width);
		height = std::min(height, info.color_attachments[i]->extend.height);
	}

	if (info.depth_stencil)
	{
		width = std::min(width, info.depth_stencil->extend.width);
		height = std::min(height, info.depth_stencil->extend.height);
	}
}

void RenderPassInfo::compute_attachment_dimensions(const RenderPassInfo& info, unsigned index, uint32_t& width, uint32_t& height)
{

}

std::vector<VkImageView> RenderPassInfo::setup_raw_views(const RenderPassInfo& info)
{
	std::vector<VkImageView> views{};
	for (unsigned i = 0; i < info.num_color_attachments; i++)
	{
		views.push_back(info.color_attachments[i]->imageView);
	}

	if (info.depth_stencil)
	{
		views.push_back(info.depth_stencil->imageView);
	}

	return views;
}

void VulkanRenderPassManager::init(VulkanEngine* engine)
{
	_renderPassesList.clear();
	_renderPassesList.resize(static_cast<uint32_t>(ERenderPassType::Max));
	for (auto& rp: _renderPassesList)
	{
		rp = std::make_unique<VulkanRenderPass>();
	}

	//ERenderPassType::Default
	{
		// the renderpass will use this color attachment.
		VkAttachmentDescription color_attachment = {};
		//the attachment will have the format needed by the swapchain
		color_attachment.format = engine->_swapchainImageFormat;
		//1 sample, we won't be doing MSAA
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		// we Clear when this attachment is loaded
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		// we keep the attachment stored when the renderpass ends
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		//we don't care about stencil
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		//we don't know or care about the starting layout of the attachment
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		//after the renderpass ends, the image has to be on a layout ready for display
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		VkAttachmentReference color_attachment_ref = {};
		//attachment number will index into the pAttachments array in the parent renderpass itself
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentDescription depth_attachment = {};
		// Depth attachment
		depth_attachment.flags = 0;
		depth_attachment.format = engine->_depthFormat;
		depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkAttachmentReference depth_attachment_ref = {};
		depth_attachment_ref.attachment = 1;
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		//we are going to create 1 subpass, which is the minimum you can do
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;
		//hook the depth attachment into the subpass
		subpass.pDepthStencilAttachment = &depth_attachment_ref;
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		VkSubpassDependency depth_dependency = {};
		depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		depth_dependency.dstSubpass = 0;
		depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depth_dependency.srcAccessMask = 0;
		depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		//array of 2 attachments, one for the color, and other for depth
		VkAttachmentDescription attachments[2] = { color_attachment,depth_attachment };
		VkSubpassDependency dependencies[2] = { dependency, depth_dependency };
		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		//2 attachments from said array
		render_pass_info.attachmentCount = 2;
		render_pass_info.pAttachments = &attachments[0];
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;
		render_pass_info.dependencyCount = 2;
		render_pass_info.pDependencies = &dependencies[0];
		get_render_pass(ERenderPassType::Default)->init(engine, render_pass_info);
	}
}

VulkanRenderPass* VulkanRenderPassManager::get_render_pass(ERenderPassType type) const
{
	return _renderPassesList[static_cast<uint32_t>(type)].get();
}
