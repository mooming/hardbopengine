// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace hbe
{
	/// @brief Type definitions for buffer operations.
	namespace BufferTypes
	{
		using TSize = size_t;
		using TIndex = TSize;
		static_assert(std::is_same<TIndex, TSize>::value);

		using TBufferData = uint8_t*;
		using TGenerateBuffer = std::function<void(TSize&, TBufferData&)>;
		using TReleaseBuffer = std::function<void(TSize, TBufferData)>;
		using TResizeBuffer = std::function<TSize(TSize)>;

	} // namespace BufferTypes

} // namespace hbe
