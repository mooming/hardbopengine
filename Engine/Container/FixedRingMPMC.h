// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <mutex>
#include <new>
#include <utility>

#include "Config/BuildConfig.h"
#include "Container/EPopResult.h"
#include "Container/EPushResult.h"
#include "Core/CommonMacros.h"


namespace hbe
{
/// API reference: docs/Container/FixedRingMPMC/index.html
template <typename T, size_t Capacity = 1024>
class FixedRingMPMC final
{
public:
	using ValueType = T;

private:
	static_assert(Capacity > 1 && (Capacity & (Capacity - 1)) == 0,
				  "Capacity must be a power of two, so a slot index is one mask and no division reaches the hot path.");

	static_assert(sizeof(T) % alignof(T) == 0,
				  "sizeof(T) must be a multiple of alignof(T), so a stride of sizeof(T) keeps every slot aligned.");

	static constexpr size_t LineSpacing = std::hardware_destructive_interference_size;
	static constexpr size_t PayloadAlignment = alignof(T);
	static constexpr size_t IndexMask = Capacity - 1;

	alignas(LineSpacing) mutable std::mutex guard;
	size_t head;
	size_t count;
	size_t published;
	alignas(PayloadAlignment) std::byte payload[Capacity * sizeof(T)];

public:
	[[nodiscard]] static constexpr size_t GetCapacity() noexcept
	{
		return Capacity;
	}

	FixedRingMPMC(const FixedRingMPMC&) = delete;
	FixedRingMPMC& operator=(const FixedRingMPMC&) = delete;

	FixedRingMPMC() noexcept
		: head(0)
		, count(0)
		, published(0)
	{
	}

	~FixedRingMPMC()
	{
		for (size_t offset = 0; offset < count; ++offset)
		{
			PayloadAt(head + offset)->~T();
		}
	}

	[[nodiscard]] EPushResult Push(const T& inItem)
	{
		const std::lock_guard lockGuard(guard);
		returnValueIf(EPushResult::Full, count == Capacity);

		new (PayloadAt(head + count)) T(inItem);
		++count;
		++published;

		return EPushResult::Pushed;
	}

	[[nodiscard]] EPushResult Push(T&& inItem)
	{
		const std::lock_guard lockGuard(guard);
		returnValueIf(EPushResult::Full, count == Capacity);

		new (PayloadAt(head + count)) T(std::move(inItem));
		++count;
		++published;

		return EPushResult::Pushed;
	}

	[[nodiscard]] EPopResult Pop(T& outItem)
	{
		const std::lock_guard lockGuard(guard);
		returnValueIf(EPopResult::Empty, count == 0);

		T* element = PayloadAt(head);
		outItem = std::move(*element);
		element->~T();

		head = (head + 1) & IndexMask;
		--count;

		return EPopResult::Element;
	}

	[[nodiscard]] bool IsEmpty() const
	{
		const std::lock_guard lockGuard(guard);

		return count == 0;
	}

	[[nodiscard]] size_t GetSlotVersion() const
	{
		const std::lock_guard lockGuard(guard);

		return published;
	}

private:
	[[nodiscard]] T* PayloadAt(size_t inIndex) noexcept
	{
		return reinterpret_cast<T*>(payload + (inIndex & IndexMask) * sizeof(T));
	}
};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
class FixedRingMPMCTest : public TestCollection
{
public:
	FixedRingMPMCTest()
		: TestCollection("FixedRingMPMCTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe

#endif // __UNIT_TEST__
