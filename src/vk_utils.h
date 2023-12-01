#pragma once

#include <vk_types.h>

namespace vk_utils
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
}