#include <vk_command_buffer.h>

#include <vk_textures.h>
#include <vk_render_pass.h>
#include <vk_framebuffer.h>
#include <vk_engine.h>
#include <vk_utils.h>

void VulkanCommandBuffer::init(VkCommandBuffer cmd)
{
	_cmd = cmd;
}

VkCommandBuffer VulkanCommandBuffer::get_cmd() const
{
	return _cmd;
}

void VulkanCommandBuffer::reset()
{
	vkResetCommandBuffer(_cmd, 0);
}

void VulkanCommandBuffer::draw_quad(std::function<void(VkCommandBuffer cmd)>&& preDraw)
{
	preDraw(_cmd);

	vkCmdDraw(_cmd, 3, 1, 0, 0);
}

void VulkanCommandBuffer::dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z, std::function<void(VkCommandBuffer cmd)>&& preDispatch)
{
	preDispatch(_cmd);

	vkCmdDispatch(_cmd, groups_x, groups_y, groups_z);
}

void VulkanCommandBuffer::draw_mesh_tasks_indirect(const AllocatedBuffer& indirectBuffer, VkDeviceSize offset, uint32_t draw_count, uint32_t stride, std::function<void(VkCommandBuffer cmd)>&& preDraw)
{
	preDraw(_cmd);

	vkCmdDrawMeshTasksIndirectNV(_cmd, indirectBuffer._buffer, offset, draw_count, stride);
}

void VulkanCommandBuffer::raytrace(const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, 
	const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, 
	uint32_t width, uint32_t height, uint32_t depth, std::function<void(VkCommandBuffer cmd)>&& preDraw)
{
	preDraw(_cmd);

	vkCmdTraceRaysKHR(_cmd, pRaygenShaderBindingTable, 
		pMissShaderBindingTable, 
		pHitShaderBindingTable, 
		pCallableShaderBindingTable,
		width, height, depth);
}

void VulkanCommandBuffer::draw_indexed_indirect(const AllocatedBuffer& indirectBuffer, VkDeviceSize offset, uint32_t draw_count, uint32_t stride, std::function<void(VkCommandBuffer cmd)>&& preDraw)
{
	preDraw(_cmd);

	vkCmdDrawIndexedIndirect(_cmd, indirectBuffer._buffer, offset, draw_count, stride);
}

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
		vkCmdClearDepthStencilImage(_cmd, image.image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			&value.depthStencil, 1, &range);
	}
	else
	{
		vkCmdClearColorImage(_cmd, image.image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
	vkutil::rect2d_transform_xy(tmp_rect.rect, surfaceTransform,
		frameWidht, frameHeight);
	vkCmdClearAttachments(_cmd, 1, &att, 1, &tmp_rect);
}

void VulkanCommandBuffer::clear_quad(const VkClearRect& rect, const VkClearAttachment* attachments, uint32_t num_attachments, uint32_t frameWidht, uint32_t frameHeight, VkSurfaceTransformFlagBitsKHR surfaceTransform)
{
	auto tmp_rect = rect;
	vkutil::rect2d_transform_xy(tmp_rect.rect, surfaceTransform,
		frameWidht, frameHeight);
	vkCmdClearAttachments(_cmd, num_attachments, attachments, 1, &tmp_rect);
}

void VulkanCommandBuffer::fill_buffer(const AllocatedBuffer& dst, uint32_t value)
{
	fill_buffer(dst, value, 0, VK_WHOLE_SIZE);
}

void VulkanCommandBuffer::fill_buffer(const AllocatedBuffer& dst, uint32_t value, VkDeviceSize offset, VkDeviceSize size)
{
	vkCmdFillBuffer(_cmd, dst._buffer, offset, size, value);
}

void VulkanCommandBuffer::copy_buffer(const AllocatedBuffer& dst, VkDeviceSize dst_offset, const AllocatedBuffer& src, VkDeviceSize src_offset, VkDeviceSize size)
{
	const VkBufferCopy region = {
	src_offset, dst_offset, size,
	};
	vkCmdCopyBuffer(_cmd, src._buffer, dst._buffer, 1, &region);
}

void VulkanCommandBuffer::copy_buffer(const AllocatedBuffer& dst, const AllocatedBuffer& src, const VkBufferCopy* copies, size_t count)
{
	vkCmdCopyBuffer(_cmd, src._buffer, dst._buffer, count, copies);
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

	vkCmdCopyImage(_cmd, src.image._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
	vkCmdPipelineBarrier(_cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
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
	vkCmdPipelineBarrier(_cmd, sync1.src_stages, sync1.dst_stages,
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

	vkCmdBlitImage(_cmd,
		src.image._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		dst.image._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit, filter);
}

void VulkanCommandBuffer::begin_render_pass(const RenderPassInfo& info, VulkanRenderPass* render_pass, VulkanFrameBuffer* framebuffer, VkSubpassContents contents)
{
	_actual_render_pass = render_pass;
	_framebuffer = framebuffer;

	uint32_t num_depth_stencil = info.depth_stencil != nullptr;
	uint32_t _framebuffer_attachments_size = info.num_color_attachments + num_depth_stencil;
	_framebuffer_attachments.resize(info.num_color_attachments + num_depth_stencil);
	for (uint32_t i = 0; i < info.num_color_attachments; i++)
		_framebuffer_attachments[i] = info.color_attachments[i];

	if (num_depth_stencil)
		_framebuffer_attachments[info.num_color_attachments] = info.depth_stencil;

	uint32_t num_clear_values = _framebuffer_attachments_size;
	std::vector<VkClearValue> clear_values;
	clear_values.resize(info.num_color_attachments + num_depth_stencil);
	for (uint32_t i = 0; i < info.num_color_attachments; i++)
		clear_values[i].color = info.clear_color[i];

	if (num_depth_stencil)
		clear_values[info.num_color_attachments].depthStencil = info.clear_depth_stencil;

	init_viewport_scissor(info, framebuffer);

	VkRenderPassBeginInfo begin_info = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	begin_info.renderPass = _actual_render_pass->get_render_pass();
	begin_info.framebuffer = _framebuffer->get_framebuffer();
	begin_info.renderArea = _scissor;
	begin_info.clearValueCount = num_clear_values;
	begin_info.pClearValues = clear_values.data();

	vkCmdBeginRenderPass(_cmd, &begin_info, contents);
}

void VulkanCommandBuffer::end_render_pass()
{
	vkCmdEndRenderPass(_cmd);

	_framebuffer = nullptr;
	_actual_render_pass = nullptr;
}

void VulkanCommandBuffer::record(VkCommandBufferUsageFlagBits flags, std::function<void()>&& func)
{
	begin(flags);

	func();

	end();
}

void VulkanCommandBuffer::begin(VkCommandBufferUsageFlagBits flags)
{
	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;

	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = flags;

	vkBeginCommandBuffer(_cmd, &cmdBeginInfo);
}

void VulkanCommandBuffer::end()
{
	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	vkEndCommandBuffer(_cmd);
}

void VulkanCommandBuffer::init_viewport_scissor(const RenderPassInfo& info,VulkanFrameBuffer* framebuffer)
{
	VkRect2D rect = info.render_area;

	uint32_t fb_width = framebuffer->get_width();
	uint32_t fb_height = framebuffer->get_height();

	rect.offset.x = std::min(int32_t(fb_width), rect.offset.x);
	rect.offset.y = std::min(int32_t(fb_height), rect.offset.y);
	rect.extent.width = std::min(fb_width - rect.offset.x, rect.extent.width);
	rect.extent.height = std::min(fb_height - rect.offset.y, rect.extent.height);

	_viewport = {
		float(rect.offset.x), float(rect.offset.y),
		float(rect.extent.width), float(rect.extent.height),
		0.0f, 1.0f
	};
	_scissor = rect;
}






