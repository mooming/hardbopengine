// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>

#include "Batch.h"
#include "Container/RingBuffer.h"
#include "Memory/AllocatorID.h"
#include "Memory/MemoryManager.h"
#include "PipelineTypes.h"


namespace hbe::pipeline
{
template <typename T>
class StageChannel final
{
public:
	const TStageID fromStageID;
	const TStageID toStageID;
	const TypeID typeID;
	const TAllocatorID allocatorID;

private:
	std::atomic<bool> isClosed;

private:
	RingBuffer<Batch, AtomicFixedRingMPSC<Batch>> queue;

public:
	StageChannel(TStageID from, TStageID to, TAllocatorID allocID)
		: fromStageID(from), toStageID(to), typeID(GetTypeID<T>()), allocatorID(allocID), isClosed(false)
	{
	}

	void Close() noexcept
	{
		isClosed.store(true, std::memory_order_release);
	}

	[[nodiscard]] bool IsClosed() const noexcept { return isClosed.load(std::memory_order_relaxed); }

	[[nodiscard]] BatchSlot<T> AllocateSlot(TSize capacity) noexcept
	{
		auto& mmgr = MemoryManager::GetInstance();
		T* ptr = static_cast<T*>(mmgr.Allocate(allocatorID, sizeof(T) * capacity));

		return BatchSlot<T>(ptr, ptr + capacity);
	}

	BatchSlot<T> Push(RequestID requestID, BatchSlot<T>&& slot, TSize itemCount) noexcept
	{
		if (isClosed.load(std::memory_order_acquire))
			return std::move(slot);

		Assert(itemCount <= slot.size());

		Batch batch {
			.typeID = typeID,
			.requestID = requestID,
			.data = slot.data(),
			.count = itemCount
		};

		auto result = queue.Push(batch);
		if (result != EPushResult::Pushed)
			return std::move(slot);

		return BatchSlot<T>();
	}

	[[nodiscard]] BatchView<T> Pop() noexcept
	{
		auto item = queue.Pop();
		if (!item)
			return {};

		Assert(item->typeID == typeID, "A batch that reached stage %u through a join carries another stage's type id.",
			   toStageID);

		return BatchView<T>{static_cast<const T*>(item->data), item->count};
	}
};
} // namespace hbe::pipeline
