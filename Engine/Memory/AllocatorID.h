// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


namespace hbe
{
/// API reference: docs/Memory/AllocatorID/index.html
using TAllocatorID = int;

static constexpr TAllocatorID MaxNumAllocators = 256;

static constexpr TAllocatorID InvalidAllocatorID = -1;

[[nodiscard]] inline bool IsValid(TAllocatorID id) noexcept
{
	return id >= 0 && id < MaxNumAllocators;
}
} // namespace hbe
