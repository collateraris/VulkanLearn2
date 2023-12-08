#pragma once

#include <vk_types.h>

class VulkanRenderPass;
struct RenderPassInfo;
class VulkanFrameBuffer;
class VulkanEngine;

class VulkanCommandBuffer
{
public:
	VulkanCommandBuffer() = default;

	void clear_image(const Texture& image, const VkClearValue& value);
	void clear_image(const Texture& image, const VkClearValue& value, VkImageAspectFlags aspect);
	void clear_quad(uint32_t attachment, const VkClearRect& rect, const VkClearValue& value,
		VkImageAspectFlags aspect,uint32_t frameWidht, uint32_t frameHeight, VkSurfaceTransformFlagBitsKHR surfaceTransform);
	void clear_quad(const VkClearRect& rect, const VkClearAttachment* attachments, uint32_t num_attachments, uint32_t frameWidht, uint32_t frameHeight, VkSurfaceTransformFlagBitsKHR surfaceTransform);

	void fill_buffer(const AllocatedBuffer& dst, uint32_t value);
	void fill_buffer(const AllocatedBuffer& dst, uint32_t value, VkDeviceSize offset, VkDeviceSize size);

	void copy_buffer(const AllocatedBuffer& dst, VkDeviceSize dst_offset, const AllocatedBuffer& src, VkDeviceSize src_offset,
		VkDeviceSize size);
	void copy_buffer(const AllocatedBuffer& dst, const AllocatedBuffer& src, const VkBufferCopy* copies, size_t count);

	void copy_image(const Texture& dst, const Texture& src);

	void full_barrier();
	void pixel_barrier();

	// Simplified global memory barrier.
	void barrier(VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
		VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access);

	void barrier(const VkDependencyInfo& dep);

	void buffer_barrier(const AllocatedBuffer& buffer,
		VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
		VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access);

	void image_barrier(const Texture& image,
		VkImageLayout old_layout, VkImageLayout new_layout,
		VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
		VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access);

	void buffer_barriers(uint32_t buffer_barriers, const VkBufferMemoryBarrier2* buffers);
	void image_barriers(uint32_t image_barriers, const VkImageMemoryBarrier2* images);

	void blit_image(const Texture& dst,
		const Texture& src,
		const VkOffset3D& dst_offset0, const VkOffset3D& dst_extent,
		const VkOffset3D& src_offset0, const VkOffset3D& src_extent, unsigned dst_level, unsigned src_level,
		unsigned dst_base_layer = 0, uint32_t src_base_layer = 0, unsigned num_layers = 1,
		VkFilter filter = VK_FILTER_LINEAR);

	void begin_render_pass(const RenderPassInfo& info, VulkanRenderPass* render_pass, VulkanFrameBuffer* framebuffer, VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);
	void end_render_pass();

private:
	VkCommandBuffer _cmd;

	VulkanFrameBuffer* _framebuffer = nullptr;
	VulkanRenderPass* _actual_render_pass = nullptr;

	std::vector<Texture*> _framebuffer_attachments{};

	VkViewport _viewport = {};
	VkRect2D _scissor = {};

	void init_viewport_scissor(const RenderPassInfo& info, VulkanFrameBuffer* framebuffer);
};