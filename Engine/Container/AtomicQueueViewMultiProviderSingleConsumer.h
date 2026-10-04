// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>

#include "Container/AtomicStackView.h"
#include "Core/CommonMacros.h"
#include "OSAL/Intrinsic.h"


namespace hbe
{
/// API reference: docs/Container/AtomicQueueViewMultiProviderSingleConsumer/index.html
template <CNext T>
class AtomicQueueViewMultiProviderSingleConsumer final
{
private:
	static_assert(std::atomic<T*>::is_always_lock_free, "The specified type is not always lock free on this platform.");

	std::atomic<T*> newest;
	T* oldest;

public:
	AtomicQueueViewMultiProviderSingleConsumer(const AtomicQueueViewMultiProviderSingleConsumer&) = delete;

	AtomicQueueViewMultiProviderSingleConsumer() noexcept
		: newest(nullptr)
		, oldest(nullptr)
	{
	}

	~AtomicQueueViewMultiProviderSingleConsumer() = default;

	AtomicQueueViewMultiProviderSingleConsumer& operator=(const AtomicQueueViewMultiProviderSingleConsumer&) = delete;

	void Push(T& newItem) noexcept
	{
		newItem.next = newest.load(std::memory_order_relaxed);

		while (!newest.compare_exchange_weak(newItem.next, &newItem, std::memory_order_release,
											 std::memory_order_relaxed))
			;
	}

	T* Pop() noexcept
	{
		if (unlikely(oldest == nullptr))
		{
			oldest = ReverseNewest();
			returnValueIf(nullptr, oldest == nullptr);
		}

		auto* node = oldest;
		oldest = node->next;
		node->next = nullptr;

		return node;
	}

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		return oldest == nullptr && newest.load(std::memory_order_relaxed) == nullptr;
	}

private:
	T* ReverseNewest() noexcept
	{
		auto* node = newest.exchange(nullptr, std::memory_order_acquire);
		T* reversed = nullptr;

		while (node != nullptr)
		{
			auto* next = node->next;
			node->next = reversed;
			reversed = node;
			node = next;
		}

		return reversed;
	}
};

template <CNext T>
using AtomicQueueViewMPSC = AtomicQueueViewMultiProviderSingleConsumer<T>;
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
class AtomicQueueViewMultiProviderSingleConsumerTest : public TestCollection
{
public:
	AtomicQueueViewMultiProviderSingleConsumerTest()
		: TestCollection("AtomicQueueViewMultiProviderSingleConsumerTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe

#endif // __UNIT_TEST__
