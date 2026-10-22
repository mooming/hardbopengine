// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <thread>
#include <utility>

#include "Config/BuildConfig.h"
#include "Container/EPopResult.h"
#include "Container/EPushResult.h"
#include "Core/CommonMacros.h"


namespace hbe
{
/// API reference: docs/Container/AtomicFixedRingMPMC/index.html
template <typename T, size_t Capacity = 1024, size_t MaxSpinAttempts = 8>
class AtomicFixedRingMPMC final
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

	alignas(LineSpacing) std::atomic<size_t> writeIndex;
	alignas(LineSpacing) std::atomic<size_t> readIndex;
	alignas(LineSpacing) std::atomic<size_t> readyIndex[Capacity];
	alignas(PayloadAlignment) std::byte payload[Capacity * sizeof(T)];

public:
	[[nodiscard]] static constexpr size_t GetCapacity() noexcept
	{
		return Capacity;
	}

	AtomicFixedRingMPMC(const AtomicFixedRingMPMC&) = delete;
	AtomicFixedRingMPMC& operator=(const AtomicFixedRingMPMC&) = delete;

	AtomicFixedRingMPMC() noexcept
		: writeIndex(0)
		, readIndex(0)
	{
		for (size_t index = 0; index < Capacity; ++index)
		{
			readyIndex[index].store(index, std::memory_order_relaxed);
		}
	}

	~AtomicFixedRingMPMC()
	{
		DestroyQueued();
	}

	[[nodiscard]] EPushResult Push(const T& inItem) noexcept
	{
		size_t claim = 0;
		returnValueIf(EPushResult::Full, !TryReserveSlot(claim));

		new (PayloadAt(claim)) T(inItem);
		MarkDataReady(claim);

		return EPushResult::Pushed;
	}

	[[nodiscard]] EPushResult Push(T&& inItem) noexcept
	{
		size_t claim = 0;
		returnValueIf(EPushResult::Full, !TryReserveSlot(claim));

		new (PayloadAt(claim)) T(std::move(inItem));
		MarkDataReady(claim);

		return EPushResult::Pushed;
	}

	[[nodiscard]] EPopResult Pop(T& outItem) noexcept
	{
		size_t taken = 0;
		returnValueIf(EPopResult::Empty, !TryTakeReadyElement(taken));

		T* element = PayloadAt(taken);
		outItem = std::move(*element);
		element->~T();

		ReleaseSlot(taken);

		return EPopResult::Element;
	}

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		return readIndex.load(std::memory_order_relaxed) == writeIndex.load(std::memory_order_relaxed);
	}

	[[nodiscard]] size_t GetSlotVersion() const noexcept
	{
		return writeIndex.load(std::memory_order_relaxed);
	}

private:
	[[nodiscard]] T* PayloadAt(size_t inIndex) noexcept
	{
		return reinterpret_cast<T*>(payload + (inIndex & IndexMask) * sizeof(T));
	}

	[[nodiscard]] bool IsDataReady(size_t inIndex) const noexcept
	{
		return readyIndex[inIndex & IndexMask].load(std::memory_order_acquire) == inIndex + 1;
	}

	void MarkDataReady(size_t inIndex) noexcept
	{
		readyIndex[inIndex & IndexMask].store(inIndex + 1, std::memory_order_release);
	}

	void ReleaseSlot(size_t inIndex) noexcept
	{
		readyIndex[inIndex & IndexMask].store(inIndex + Capacity, std::memory_order_release);
	}

	[[nodiscard]] bool TryReserveSlot(size_t& outIndex) noexcept
	{
		size_t position = writeIndex.load(std::memory_order_relaxed);
		size_t attempts = 0;

		while (true)
		{
			const size_t slotReadyIndex = readyIndex[position & IndexMask].load(std::memory_order_acquire);
			if (position == slotReadyIndex)
			{
				if (writeIndex.compare_exchange_weak(position, position + 1, std::memory_order_relaxed,
													 std::memory_order_relaxed))
				{
					outIndex = position;

					return true;
				}
			}
			else if (slotReadyIndex < position)
			{
				return false;
			}
			else
			{
				position = writeIndex.load(std::memory_order_relaxed);
			}

			if (++attempts > MaxSpinAttempts)
			{
				std::this_thread::yield();
			}
		}
	}

	[[nodiscard]] bool TryTakeReadyElement(size_t& outIndex) noexcept
	{
		size_t position = readIndex.load(std::memory_order_relaxed);
		size_t attempts = 0;

		while (true)
		{
			const size_t slotReadyIndex = readyIndex[position & IndexMask].load(std::memory_order_acquire);
			if (slotReadyIndex == position + 1)
			{
				if (readIndex.compare_exchange_weak(position, position + 1, std::memory_order_relaxed,
													std::memory_order_relaxed))
				{
					outIndex = position;

					return true;
				}
			}
			else if (slotReadyIndex < position + 1)
			{
				return false;
			}
			else
			{
				position = readIndex.load(std::memory_order_relaxed);
			}

			if (++attempts > MaxSpinAttempts)
			{
				std::this_thread::yield();
			}
		}
	}

	void DestroyQueued() noexcept
	{
		const size_t head = readIndex.load(std::memory_order_relaxed);
		const size_t tail = writeIndex.load(std::memory_order_relaxed);

		for (size_t index = head; index < tail; ++index)
		{
			continueIf(!IsDataReady(index));

			PayloadAt(index)->~T();
		}
	}
};
} // namespace hbe

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class AtomicFixedRingMPMCTest : public TestCollection
{
public:
	AtomicFixedRingMPMCTest()
		: TestCollection("AtomicFixedRingMPMCTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe

#endif // TEST_ENABLED
