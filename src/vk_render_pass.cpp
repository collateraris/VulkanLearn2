#include <vk_render_pass.h>

#include <vk_utils.h>
#include <vk_engine.h>

void VulkanRenderPass::init(VulkanEngine* engine, const VkRenderPassCreateInfo& create_info_)
{
	create_info = create_info_;

	setup_subpasses(create_info);

	engine->create_render_pass(create_info, render_pass);
}

void VulkanRenderPass::init(VulkanEngine* engine, const RenderPassInfo& info)
{
	std::fill(std::begin(color_attachments), std::end(color_attachments), VK_FORMAT_UNDEFINED);

	const RenderPassInfo::Subpass* subpassInfo = info.subpasses;
	uint32_t num_subpasses = info.num_subpasses;
	RenderPassInfo::Subpass default_subpass_info;

	if (!info.subpasses)
	{
		default_subpass_info.num_color_attachments = info.num_color_attachments;
		if (info.op_flags & RENDER_PASS_OP_DEPTH_STENCIL_READ_ONLY_BIT)
			default_subpass_info.depth_stencil_mode = RenderPassInfo::DepthStencil::ReadOnly;
		else
			default_subpass_info.depth_stencil_mode = RenderPassInfo::DepthStencil::ReadWrite;

		for (uint32_t i = 0; i < info.num_color_attachments; i++)
			default_subpass_info.color_attachments[i] = i;
		num_subpasses = 1;
		subpassInfo = &default_subpass_info;
	}

	const uint32_t num_attachments = info.num_color_attachments + (info.depth_stencil ? 1 : 0);
	std::array<VkAttachmentDescription, VULKAN_NUM_ATTACHMENTS + 1> attachments;

	VkAttachmentLoadOp ds_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	VkAttachmentStoreOp ds_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	const auto color_load_op = [&info](uint32_t index) -> VkAttachmentLoadOp {
		if ((info.clear_attachments & (1u << index)) != 0)
			return VK_ATTACHMENT_LOAD_OP_CLEAR;
		else if ((info.load_attachments & (1u << index)) != 0)
			return VK_ATTACHMENT_LOAD_OP_LOAD;
		else
			return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	};

	const auto color_store_op = [&info](uint32_t index) -> VkAttachmentStoreOp {
		if ((info.store_attachments & (1u << index)) != 0)
			return VK_ATTACHMENT_STORE_OP_STORE;
		else
			return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	};

	if (info.op_flags & RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT)
		ds_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
	else if (info.op_flags & RENDER_PASS_OP_LOAD_DEPTH_STENCIL_BIT)
		ds_load_op = VK_ATTACHMENT_LOAD_OP_LOAD;

	if (info.op_flags & RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT)
		ds_store_op = VK_ATTACHMENT_STORE_OP_STORE;

	bool ds_read_only = (info.op_flags & RENDER_PASS_OP_DEPTH_STENCIL_READ_ONLY_BIT) != 0;
	VkImageLayout depth_stencil_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (info.depth_stencil)
	{
		depth_stencil_layout = 
			ds_read_only ?
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	for (uint32_t i = 0; i < info.num_color_attachments; i++)
	{
		color_attachments[i] = info.color_attachments[i]->createInfo.format;
		Texture& image = *info.color_attachments[i];
		auto& attachment = attachments[i];
		attachment = { VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR };
		attachment.format = color_attachments[i];
		attachment.samples = image.createInfo.samples;
		attachment.loadOp = color_load_op(i);
		attachment.storeOp = color_store_op(i);
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// Undefined final layout here for now means that we will just use the layout of the last
		// subpass which uses this attachment to avoid any dummy transition at the end.
		attachment.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (image.bIsSwapChainImage)
		{
			if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
				attachment.initialLayout = image.createInfo.initialLayout;
			else
				attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			attachment.finalLayout = image.createInfo.initialLayout;
		}
		else
			attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	depth_stencil = info.depth_stencil ? info.depth_stencil->createInfo.format : VK_FORMAT_UNDEFINED;
	if (info.depth_stencil)
	{
		Texture& image = *info.depth_stencil;
		auto& attachment = attachments[info.num_color_attachments];
		attachment = { VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR };
		attachment.format = depth_stencil;
		attachment.samples = image.createInfo.samples;
		attachment.loadOp = ds_load_op;
		attachment.storeOp = ds_store_op;
		// Undefined final layout here for now means that we will just use the layout of the last
		// subpass which uses this attachment to avoid any dummy transition at the end.
		attachment.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (vkutil::format_to_aspect_mask(depth_stencil) & VK_IMAGE_ASPECT_STENCIL_BIT)
		{
			attachment.stencilLoadOp = ds_load_op;
			attachment.stencilStoreOp = ds_store_op;
		}
		else
		{
			attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}

		attachment.initialLayout = depth_stencil_layout;
	}


	std::vector<VkSubpassDescription> subpasses(num_subpasses);
	std::vector<std::vector<VkAttachmentReference>> colorsReserve(num_subpasses);
	std::vector<VkAttachmentReference> depthReserve(num_subpasses);

	for (uint32_t i = 0; i < num_subpasses; i++)
	{
		colorsReserve[i].resize(subpassInfo[i].num_color_attachments);
	}

	for (uint32_t i = 0; i < num_subpasses; i++)
	{
		std::vector<VkAttachmentReference>& colors = colorsReserve[i];
		VkAttachmentReference& depth = depthReserve[i];

		VkSubpassDescription& subpass = subpasses[i];
		subpass = { VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR };
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = subpassInfo[i].num_color_attachments;
		subpass.pColorAttachments = colors.data();
		subpass.inputAttachmentCount = subpassInfo[i].num_input_attachments;
		subpass.pDepthStencilAttachment = &depth;

		for (uint32_t j = 0; j < subpass.colorAttachmentCount; j++)
		{
			auto att = subpassInfo[i].color_attachments[j];
			colors[j].attachment = att;
			// Fill in later.
			colors[j].layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}

		if (info.depth_stencil && subpassInfo[i].depth_stencil_mode != RenderPassInfo::DepthStencil::None)
		{
			depth.attachment = info.num_color_attachments;
		}
		else
		{
			depth.attachment = VK_ATTACHMENT_UNUSED;
		}

		depth.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	const auto find_color = [&](uint32_t subpass, uint32_t attachment) -> VkAttachmentReference* {
		auto* colors = subpasses[subpass].pColorAttachments;
		for (uint32_t i = 0; i < subpasses[subpass].colorAttachmentCount; i++)
			if (colors[i].attachment == attachment)
				return const_cast<VkAttachmentReference*>(&colors[i]);
		return nullptr;
	};

	const auto find_depth_stencil = [&](uint32_t subpass, uint32_t attachment) -> VkAttachmentReference* {
		if (subpasses[subpass].pDepthStencilAttachment->attachment == attachment)
			return const_cast<VkAttachmentReference*>(subpasses[subpass].pDepthStencilAttachment);
		else
			return nullptr;
	};

	// Last subpass which makes use of an attachment.
	std::array<uint32_t, VULKAN_NUM_ATTACHMENTS + 1> last_subpass_for_attachment= {};

	// 1 << subpass bit set if any input attachment is read in the subpass.
	uint32_t color_attachment_read_write = 0;
	uint32_t depth_stencil_attachment_write = 0;
	uint32_t depth_stencil_attachment_read = 0;

	uint32_t external_color_dependencies = 0;
	uint32_t external_depth_dependencies = 0;

	for (uint32_t attachment = 0; attachment < num_attachments; attachment++)
	{
		bool used = false;
		VkImageLayout current_layout = attachments[attachment].initialLayout;
		for (uint32_t subpass = 0; subpass < num_subpasses; subpass++)
		{
			auto* color = find_color(subpass, attachment);
			auto* depth = find_depth_stencil(subpass, attachment);

			if(color) // No particular preference
			{
				if (current_layout != VK_IMAGE_LAYOUT_GENERAL)
					current_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				color->layout = current_layout;

				// If first subpass changes the layout, we'll need to inject an external subpass dependency.
				if (!used && attachments[attachment].initialLayout != current_layout)
					external_color_dependencies |= 1u << subpass;

				used = true;
				last_subpass_for_attachment[attachment] = subpass;
				color_attachment_read_write |= 1u << subpass;
			}
			else if (depth)
			{
				if (subpassInfo[subpass].depth_stencil_mode == RenderPassInfo::DepthStencil::ReadWrite)
				{
					depth_stencil_attachment_write |= 1u << subpass;
					if (current_layout != VK_IMAGE_LAYOUT_GENERAL)
						current_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
				else
				{
					if (current_layout != VK_IMAGE_LAYOUT_GENERAL)
						current_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				}

				// If first subpass changes the layout, we'll need to inject an external subpass dependency.
				if (!used && attachments[attachment].initialLayout != current_layout)
					external_depth_dependencies |= 1u << subpass;

				depth_stencil_attachment_read |= 1u << subpass;
				depth->layout = current_layout;
				used = true;
				last_subpass_for_attachment[attachment] = subpass;
			}

			// If we don't have a specific layout we need to end up in, just
			// use the last one.
			// Assert that we actually use all the attachments we have ...
			if (attachments[attachment].finalLayout == VK_IMAGE_LAYOUT_UNDEFINED)
			{
				attachments[attachment].finalLayout = current_layout;
			}
		}
	}

	VkRenderPassCreateInfo rp_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rp_info.subpassCount = num_subpasses;
	rp_info.pSubpasses = subpasses.data();
	rp_info.attachmentCount = num_attachments;
	rp_info.pAttachments = attachments.data();

	std::vector<VkSubpassDependency> external_dependencies;

	// Add external subpass dependencies.
	for_each_bit(external_color_dependencies | external_depth_dependencies,
		[&](uint32_t subpass) {
			external_dependencies.emplace_back();
			auto& dep = external_dependencies.back();
			dep.srcSubpass = VK_SUBPASS_EXTERNAL;
			dep.dstSubpass = subpass;

			if (external_color_dependencies & (1u << subpass))
			{
				dep.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dep.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dep.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				dep.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			}

			if (external_depth_dependencies & (1u << subpass))
			{
				dep.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				dep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				dep.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				dep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			}
		});

	// Flush and invalidate caches between each subpass.
	for (uint32_t subpass = 1; subpass < num_subpasses; subpass++)
	{
		external_dependencies.emplace_back();
		auto& dep = external_dependencies.back();
		dep.srcSubpass = subpass - 1;
		dep.dstSubpass = subpass;
		dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		if (color_attachment_read_write & (1u << (subpass - 1)))
		{
			dep.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dep.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		if (depth_stencil_attachment_write & (1u << (subpass - 1)))
		{
			dep.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dep.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		if (color_attachment_read_write & (1u << subpass))
		{
			dep.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dep.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		}

		if (depth_stencil_attachment_read & (1u << subpass))
		{
			dep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}

		if (depth_stencil_attachment_write & (1u << subpass))
		{
			dep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
	}

	if (!external_dependencies.empty())
	{
		rp_info.dependencyCount = external_dependencies.size();
		rp_info.pDependencies = external_dependencies.data();
	}

	create_info = rp_info;

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
		memcpy(subpass_info.color_attachments.data(), subpass.pColorAttachments,
			subpass.colorAttachmentCount * sizeof(*subpass.pColorAttachments));
		memcpy(subpass_info.input_attachments.data(), subpass.pInputAttachments,
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

	for (uint32_t i = 0; i < info.num_color_attachments; i++)
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

void RenderPassInfo::compute_attachment_dimensions(const RenderPassInfo& info, uint32_t index, uint32_t& width, uint32_t& height)
{

}

std::vector<VkImageView> RenderPassInfo::setup_raw_views(const RenderPassInfo& info)
{
	std::vector<VkImageView> views{};
	for (uint32_t i = 0; i < info.num_color_attachments; i++)
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
}

VulkanRenderPass* VulkanRenderPassManager::get_render_pass(ERenderPassType type) const
{
	return _renderPassesList[static_cast<uint32_t>(type)].get();
}
