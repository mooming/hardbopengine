// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "Config/BuildConfig.h"


namespace hbe::pipeline
{
using TStageID = uint32_t;
using TypeID = std::uintptr_t;
using RequestID = uint64_t;
using TSize = std::size_t;

template <typename T>
using BatchSlot = std::span<T>;

template <typename T>
using BatchView = std::span<const T>;

template <typename T>
[[nodiscard]] TypeID GetTypeID() noexcept
{
#if PLATFORM_WINDOWS
	return reinterpret_cast<TypeID>(__FUNCSIG__);
#else // PLATFORM_WINDOWS
	return reinterpret_cast<TypeID>(__PRETTY_FUNCTION__);
#endif // PLATFORM_WINDOWS
}
} // namespace hbe::pipeline