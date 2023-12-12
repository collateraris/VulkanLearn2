#pragma once

#include <vk_types.h>

namespace vkutil
{
	template <Integral T>
	constexpr bool is_aligned(T x, size_t a) noexcept
	{
		return (x & (T(a) - 1)) == 0;
	};

	template <Integral T>
	constexpr T align_up(T x, size_t a) noexcept
	{
		return T((x + (T(a) - 1)) & ~T(a - 1));
	};

	template <Integral T>
	constexpr T align_down(T x, size_t a) noexcept
	{
		return T(x & ~T(a - 1));
	};

	template <Integral T>
	T padSizeToAlignment(T originalSize, T minAlignment)
	{
		return (originalSize + minAlignment - 1) & ~(minAlignment - 1);
	};

	static inline bool format_has_depth_aspect(VkFormat format)
	{
		switch (format)
		{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;

		default:
			return false;
		}
	}

	static inline bool format_has_stencil_aspect(VkFormat format)
	{
		switch (format)
		{
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_S8_UINT:
			return true;

		default:
			return false;
		}
	}

	static inline bool format_has_depth_or_stencil_aspect(VkFormat format)
	{
		return format_has_depth_aspect(format) || format_has_stencil_aspect(format);
	}

	// FIXME: Also consider that we might have to flip X or Y w.r.t. dimensions,
	// but that only matters for partial rendering ...
	static inline bool surface_transform_swaps_xy(VkSurfaceTransformFlagBitsKHR transform)
	{
		return (transform & (
			VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR |
			VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR |
			VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR |
			VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)) != 0;
	}

	static inline void rect2d_transform_xy(VkRect2D &rect, VkSurfaceTransformFlagBitsKHR transform,
                                       uint32_t fb_width, uint32_t fb_height)
{
	switch (transform)
	{
	case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
	{
		int new_y = rect.offset.x;
		int new_x = int(fb_width) - int(rect.offset.y + rect.extent.height);
		rect.offset = { new_x, new_y };
		std::swap(rect.extent.width, rect.extent.height);
		break;
	}

	case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
	{
		// Untested. Cannot make Android trigger this mode.
		int new_left = int(fb_width) - int(rect.offset.x + rect.extent.width);
		int new_top = int(fb_height) - int(rect.offset.y + rect.extent.height);
		rect.offset = { new_left, new_top };
		break;
	}

	case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
	{
		int new_x = rect.offset.y;
		int new_y = int(fb_height) - int(rect.offset.x + rect.extent.width);
		rect.offset = { new_x, new_y };
		std::swap(rect.extent.width, rect.extent.height);
		break;
	}

	default:
		break;
	}
}
	static inline VkPipelineStageFlags convert_vk_stage2(VkPipelineStageFlags2 stages)
	{
		constexpr VkPipelineStageFlags2 transfer_mask =
			VK_PIPELINE_STAGE_2_COPY_BIT |
			VK_PIPELINE_STAGE_2_BLIT_BIT |
			VK_PIPELINE_STAGE_2_RESOLVE_BIT |
			VK_PIPELINE_STAGE_2_CLEAR_BIT |
			VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR;

		constexpr VkPipelineStageFlags2 preraster_mask =
			VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT;

		if ((stages & transfer_mask) != 0)
		{
			stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			stages &= ~transfer_mask;
		}

		if ((stages & preraster_mask) != 0)
		{
			// TODO: Augment if we add mesh shader support eventually.
			stages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			stages &= ~preraster_mask;
		}

		return VkPipelineStageFlags(stages);
	}

	static inline VkAccessFlags convert_vk_access_flags2(VkAccessFlags2 access)
	{
		constexpr VkAccessFlags2 sampled_mask =
			VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
			VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR;

		constexpr VkAccessFlags2 storage_mask =
			VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

		if ((access & sampled_mask) != 0)
		{
			access |= VK_ACCESS_SHADER_READ_BIT;
			access &= ~sampled_mask;
		}

		if ((access & storage_mask) != 0)
		{
			access |= VK_ACCESS_SHADER_WRITE_BIT;
			access &= ~storage_mask;
		}

		return VkAccessFlags(access);
	}

	static inline VkPipelineStageFlags convert_vk_src_stage2(VkPipelineStageFlags2 stages)
	{
		stages = convert_vk_stage2(stages);
		if (stages == VK_PIPELINE_STAGE_NONE)
			stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		return VkPipelineStageFlags(stages);
	}

	static inline VkPipelineStageFlags convert_vk_dst_stage2(VkPipelineStageFlags2 stages)
	{
		stages = convert_vk_stage2(stages);
		if (stages == VK_PIPELINE_STAGE_NONE)
			stages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		return VkPipelineStageFlags(stages);
	}

	struct Sync1CompatData
	{
		std::vector<VkMemoryBarrier> mem_barriers;
		std::vector<VkBufferMemoryBarrier> buf_barriers;
		std::vector<VkImageMemoryBarrier> img_barriers;
		VkPipelineStageFlags src_stages = 0;
		VkPipelineStageFlags dst_stages = 0;
	};

	static void convert_vk_dependency_info(const VkDependencyInfo& dep, Sync1CompatData& sync1)
	{
		VkPipelineStageFlags2 src_stages = 0;
		VkPipelineStageFlags2 dst_stages = 0;

		for (uint32_t i = 0; i < dep.memoryBarrierCount; i++)
		{
			auto& mem = dep.pMemoryBarriers[i];
			src_stages |= mem.srcStageMask;
			dst_stages |= mem.dstStageMask;
			VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			barrier.srcAccessMask = convert_vk_access_flags2(mem.srcAccessMask);
			barrier.dstAccessMask = convert_vk_access_flags2(mem.dstAccessMask);
			sync1.mem_barriers.push_back(barrier);
		}

		for (uint32_t i = 0; i < dep.bufferMemoryBarrierCount; i++)
		{
			auto& buf = dep.pBufferMemoryBarriers[i];
			src_stages |= buf.srcStageMask;
			dst_stages |= buf.dstStageMask;

			VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
			barrier.srcAccessMask = convert_vk_access_flags2(buf.srcAccessMask);
			barrier.dstAccessMask = convert_vk_access_flags2(buf.dstAccessMask);
			barrier.buffer = buf.buffer;
			barrier.offset = buf.offset;
			barrier.size = buf.size;
			barrier.srcQueueFamilyIndex = buf.srcQueueFamilyIndex;
			barrier.dstQueueFamilyIndex = buf.dstQueueFamilyIndex;
			sync1.buf_barriers.push_back(barrier);
		}

		for (uint32_t i = 0; i < dep.imageMemoryBarrierCount; i++)
		{
			auto& img = dep.pImageMemoryBarriers[i];
			VK_ASSERT(img.newLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			src_stages |= img.srcStageMask;
			dst_stages |= img.dstStageMask;

			VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			barrier.srcAccessMask = convert_vk_access_flags2(img.srcAccessMask);
			barrier.dstAccessMask = convert_vk_access_flags2(img.dstAccessMask);
			barrier.image = img.image;
			barrier.subresourceRange = img.subresourceRange;
			barrier.oldLayout = img.oldLayout;
			barrier.newLayout = img.newLayout;
			barrier.srcQueueFamilyIndex = img.srcQueueFamilyIndex;
			barrier.dstQueueFamilyIndex = img.dstQueueFamilyIndex;
			sync1.img_barriers.push_back(barrier);
		}

		sync1.src_stages |= convert_vk_src_stage2(src_stages);
		sync1.dst_stages |= convert_vk_dst_stage2(dst_stages);
	}
}