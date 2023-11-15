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
	T padSizeToMinAlignment(T originalSize, T minAlignment)
	{
		return (originalSize + minAlignment - 1) & ~(minAlignment - 1);
	};
}