// Created by mooming.go@gmail.com, 2022

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace HE
{

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

} // namespace HE
