// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "Core/CommonMacros.h"
#include "OSAL/Intrinsic.h"


namespace hbe
{
/// API reference: docs/Memory/AlignedStorage/index.html
template <typename TObject>
struct AlignedStorage final
{
	std::byte* raw = nullptr;
	TObject* aligned = nullptr;
	size_t bytes = 0;

	template <typename TAllocator>
	[[nodiscard]] static AlignedStorage Allocate(TAllocator& inAllocator, const size_t inCount) noexcept
	{
		AlignedStorage storage;
		storage.bytes = sizeof(TObject) * inCount + alignof(TObject);
		storage.raw = reinterpret_cast<std::byte*>(inAllocator.allocate(storage.bytes));
		returnValueIf(storage, unlikely(storage.raw == nullptr));

		const auto address = reinterpret_cast<uintptr_t>(storage.raw);
		const uintptr_t aligned = (address + alignof(TObject) - 1) & ~static_cast<uintptr_t>(alignof(TObject) - 1);
		storage.aligned = reinterpret_cast<TObject*>(aligned);

		return storage;
	}

	template <typename TAllocator>
	void Free(TAllocator& inAllocator) const noexcept
	{
		returnIf(raw == nullptr);

		using BytePointer = std::remove_pointer_t<decltype(std::declval<TAllocator&>().allocate(1))>;
		inAllocator.deallocate(reinterpret_cast<BytePointer*>(raw), bytes);
	}
};
} // namespace hbe
