#include <vk_command_buffer.h>

#include <vk_textures.h>
#include <vk_utils.h>

void VulkanCommandBuffer::clear_image(const Texture& image, const VkClearValue& value)
{
	VkImageAspectFlags aspect = vkutil::format_to_aspect_mask(image.createInfo.format);
	clear_image(image, value, aspect);
}

void VulkanCommandBuffer::clear_image(const Texture& image, const VkClearValue& value, VkImageAspectFlags aspect)
{
	VkImageSubresourceRange range = {};
	range.aspectMask = aspect;
	range.baseArrayLayer = 0;
	range.baseMipLevel = 0;
	range.levelCount = image.createInfo.mipLevels;
	range.layerCount = image.createInfo.arrayLayers;
	if (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
	{
		vkCmdClearDepthStencilImage(cmd, image.image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			&value.depthStencil, 1, &range);
	}
	else
	{
		vkCmdClearColorImage(cmd, image.image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			&value.color, 1, &range);
	}
}

void VulkanCommandBuffer::clear_quad(uint32_t attachment, const VkClearRect& rect, 
	const VkClearValue& value, VkImageAspectFlags aspect, 
	uint32_t frameWidht, uint32_t frameHeight, VkSurfaceTransformFlagBitsKHR surfaceTransform)
{
	VkClearAttachment att = {};
	att.clearValue = value;
	att.colorAttachment = attachment;
	att.aspectMask = aspect;

	auto tmp_rect = rect;
	vk_utils::rect2d_transform_xy(tmp_rect.rect, surfaceTransform,
		frameWidht, frameHeight);
	vkCmdClearAttachments(cmd, 1, &att, 1, &tmp_rect);
}

void VulkanCommandBuffer::clear_quad(const VkClearRect& rect, const VkClearAttachment* attachments, uint32_t num_attachments, uint32_t frameWidht, uint32_t frameHeight, VkSurfaceTransformFlagBitsKHR surfaceTransform)
{
	auto tmp_rect = rect;
	vk_utils::rect2d_transform_xy(tmp_rect.rect, surfaceTransform,
		frameWidht, frameHeight);
	vkCmdClearAttachments(cmd, num_attachments, attachments, 1, &tmp_rect);
}

void VulkanCommandBuffer::fill_buffer(const AllocatedBuffer& dst, uint32_t value)
{
	fill_buffer(dst, value, 0, VK_WHOLE_SIZE);
}

void VulkanCommandBuffer::fill_buffer(const AllocatedBuffer& dst, uint32_t value, VkDeviceSize offset, VkDeviceSize size)
{
	vkCmdFillBuffer(cmd, dst._buffer, offset, size, value);
}

void VulkanCommandBuffer::copy_buffer(const AllocatedBuffer& dst, VkDeviceSize dst_offset, const AllocatedBuffer& src, VkDeviceSize src_offset, VkDeviceSize size)
{
	const VkBufferCopy region = {
	src_offset, dst_offset, size,
	};
	vkCmdCopyBuffer(cmd, src._buffer, dst._buffer, 1, &region);
}

void VulkanCommandBuffer::copy_buffer(const AllocatedBuffer& dst, const AllocatedBuffer& src)
{
	copy_buffer(dst, 0, src, 0, dst._allocation->GetSize());
}

void VulkanCommandBuffer::copy_buffer(const AllocatedBuffer& dst, const AllocatedBuffer& src, const VkBufferCopy* copies, size_t count)
{
	vkCmdCopyBuffer(cmd, src._buffer, dst._buffer, count, copies);
}

void VulkanCommandBuffer::copy_image(const Texture& dst, const Texture& src)
{
	const uint32_t levels = src.createInfo.mipLevels;

	std::vector<VkImageCopy> regions{};

	for (uint32_t i = 0; i < levels; i++)
	{
		VkImageCopy region;
		region.extent.width = src.createInfo.extent.width;
		region.extent.height = src.createInfo.extent.height;
		region.extent.depth = src.createInfo.extent.depth;
		region.srcSubresource.aspectMask = vkutil::format_to_aspect_mask(src.createInfo.format);
		region.srcSubresource.layerCount = src.createInfo.arrayLayers;
		region.dstSubresource.aspectMask = vkutil::format_to_aspect_mask(dst.createInfo.format);
		region.dstSubresource.layerCount = dst.createInfo.arrayLayers;
		region.srcSubresource.mipLevel = i;
		region.dstSubresource.mipLevel = i;
		regions.push_back(region);
	}

	vkCmdCopyImage(cmd, src.image._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst.image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		levels, regions.data());
}

void VulkanCommandBuffer::full_barrier()
{
	barrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT);
}

void VulkanCommandBuffer::pixel_barrier()
{
	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_DEPENDENCY_BY_REGION_BIT, 1, &barrier, 0, nullptr, 0, nullptr);
}

void VulkanCommandBuffer::barrier(VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
	VkDependencyInfo dep = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	VkMemoryBarrier2 b = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
	dep.memoryBarrierCount = 1;
	dep.pMemoryBarriers = &b;
	b.srcStageMask = src_stage;
	b.dstStageMask = dst_stage;
	b.srcAccessMask = src_access;
	b.dstAccessMask = dst_access;
	barrier(dep);
}

void VulkanCommandBuffer::barrier(const VkDependencyInfo& dep)
{
	vkutil::Sync1CompatData sync1;
	vkutil::convert_vk_dependency_info(dep, sync1);
	vkCmdPipelineBarrier(cmd, sync1.src_stages, sync1.dst_stages,
		dep.dependencyFlags,
		uint32_t(sync1.mem_barriers.size()), sync1.mem_barriers.data(),
		uint32_t(sync1.buf_barriers.size()), sync1.buf_barriers.data(),
		uint32_t(sync1.img_barriers.size()), sync1.img_barriers.data());
}

void VulkanCommandBuffer::buffer_barrier(const AllocatedBuffer& buffer, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
	VkBufferMemoryBarrier2 b = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
	VkDependencyInfo dep = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

	b.srcAccessMask = src_access;
	b.dstAccessMask = dst_access;
	b.buffer = buffer._buffer;
	b.offset = 0;
	b.size = VK_WHOLE_SIZE;
	b.srcStageMask = src_stage;
	b.dstStageMask = dst_stage;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	dep.bufferMemoryBarrierCount = 1;
	dep.pBufferMemoryBarriers = &b;

	barrier(dep);
}

void VulkanCommandBuffer::image_barrier(const Texture& image, VkImageLayout old_layout, VkImageLayout new_layout, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
	VkImageMemoryBarrier2 b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	b.srcAccessMask = src_access;
	b.dstAccessMask = dst_access;
	b.oldLayout = old_layout;
	b.newLayout = new_layout;
	b.image = image.image._image;
	b.subresourceRange.aspectMask = vkutil::format_to_aspect_mask(image.createInfo.format);
	b.subresourceRange.levelCount = image.createInfo.mipLevels;
	b.subresourceRange.layerCount = image.createInfo.arrayLayers;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.srcStageMask = src_stage;
	b.dstStageMask = dst_stage;

	image_barriers(1, &b);
}

void VulkanCommandBuffer::buffer_barriers(uint32_t buffer_barriers, const VkBufferMemoryBarrier2* buffers)
{
	VkDependencyInfo dep = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dep.bufferMemoryBarrierCount = buffer_barriers;
	dep.pBufferMemoryBarriers = buffers;
	barrier(dep);
}

void VulkanCommandBuffer::image_barriers(uint32_t image_barriers, const VkImageMemoryBarrier2* images)
{
	VkDependencyInfo dep = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
	dep.imageMemoryBarrierCount = image_barriers;
	dep.pImageMemoryBarriers = images;
	barrier(dep);
}

void VulkanCommandBuffer::blit_image(const Texture& dst, const Texture& src, const VkOffset3D& dst_offset, const VkOffset3D& dst_extent, const VkOffset3D& src_offset, const VkOffset3D& src_extent, unsigned dst_level, unsigned src_level, unsigned dst_base_layer, uint32_t src_base_layer, unsigned num_layers, VkFilter filter)
{
	const auto add_offset = [](const VkOffset3D& a, const VkOffset3D& b) -> VkOffset3D {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	};

	const VkImageBlit blit = {
		{ vkutil::format_to_aspect_mask(src.createInfo.format), src_level, src_base_layer, num_layers },
		{ src_offset, add_offset(src_offset, src_extent) },
		{ vkutil::format_to_aspect_mask(dst.createInfo.format), dst_level, dst_base_layer, num_layers },
		{ dst_offset, add_offset(dst_offset, dst_extent) },
	};

	vkCmdBlitImage(cmd,
		src.image._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst.image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit, filter);
}






