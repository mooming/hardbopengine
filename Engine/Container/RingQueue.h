// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <utility>

#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"


namespace hbe
{

	template<typename TElement, class TAllocator = DefaultAllocator<TElement>>
	class RingQueue final
	{
	public:
		using TIndex = int;

		RingQueue(const RingQueue&) = delete;

		explicit RingQueue(TIndex fixedCapacity)
			: cap(0)
			, head(0)
			, tail(0)
			, count(0)
			, data(nullptr)
		{
			FatalAssert(fixedCapacity > 0, "RingQueue capacity must be positive");

			TAllocator alloc;
			data = alloc.allocate(fixedCapacity);
			cap = fixedCapacity;
		}

		RingQueue(RingQueue&& rhs) noexcept
			: cap(0)
			, head(0)
			, tail(0)
			, count(0)
			, data(nullptr)
		{
			Swap(rhs);
		}

		~RingQueue()
		{
			if (data == nullptr)
				return;

			DestroyAll();
			TAllocator alloc;
			alloc.deallocate(data, cap);
		}

		RingQueue& operator=(const RingQueue&) = delete;

		RingQueue& operator=(RingQueue&& rhs) noexcept
		{
			Swap(rhs);
			return *this;
		}

		TElement& operator[](TIndex index) noexcept
		{
			FatalAssert(IsValidIndex(index));
			return data[WrapIndex(head + index)];
		}

		const TElement& operator[](TIndex index) const noexcept
		{
			FatalAssert(IsValidIndex(index));
			return data[WrapIndex(head + index)];
		}

		void Push(const TElement& value) noexcept
		{
			FatalAssert(!IsFull(), "RingQueue is full");
			new (&data[tail]) TElement(value);
			tail = WrapIndex(tail + 1);
			++count;
		}

		void Push(TElement&& value) noexcept
		{
			FatalAssert(!IsFull(), "RingQueue is full");
			new (&data[tail]) TElement(std::move(value));
			tail = WrapIndex(tail + 1);
			++count;
		}

		template<typename... Types>
		TElement& Emplace(Types&&... args) noexcept
		{
			FatalAssert(!IsFull(), "RingQueue is full");
			auto* ptr = new (&data[tail]) TElement(std::forward<Types>(args)...);
			tail = WrapIndex(tail + 1);
			++count;
			return *ptr;
		}

		TElement Pop() noexcept
		{
			FatalAssert(!IsEmpty(), "RingQueue is empty");
			auto item = std::move(data[head]);
			data[head].~TElement();
			head = WrapIndex(head + 1);
			--count;

			return item;
		}

		TElement& Front() noexcept
		{
			FatalAssert(!IsEmpty());
			return data[head];
		}

		const TElement& Front() const noexcept
		{
			FatalAssert(!IsEmpty());
			return data[head];
		}

		TElement& Back() noexcept
		{
			FatalAssert(!IsEmpty());
			return data[WrapIndex(tail - 1)];
		}

		const TElement& Back() const noexcept
		{
			FatalAssert(!IsEmpty());
			return data[WrapIndex(tail - 1)];
		}

		[[nodiscard]] TIndex Size() const noexcept { return count; }
		[[nodiscard]] TIndex Capacity() const noexcept { return cap; }
		[[nodiscard]] bool IsEmpty() const noexcept { return count == 0; }
		[[nodiscard]] bool IsFull() const noexcept { return count == cap; }
		[[nodiscard]] bool IsValidIndex(TIndex index) const noexcept { return index >= 0 && index < Size(); }

		void Clear() noexcept
		{
			DestroyAll();
			head = 0;
			tail = 0;
			count = 0;
		}

		void Swap(RingQueue& rhs) noexcept
		{
			std::swap(cap, rhs.cap);
			std::swap(head, rhs.head);
			std::swap(tail, rhs.tail);
			std::swap(count, rhs.count);
			std::swap(data, rhs.data);
		}

	private:
		TIndex cap;
		TIndex head;
		TIndex tail;
		TIndex count;
		TElement* data;

		TIndex WrapIndex(TIndex index) const noexcept { return index % cap; }

		void DestroyAll() noexcept
		{
			for (TIndex i = 0; i < count; ++i)
			{
				auto idx = WrapIndex(head + i);
				data[idx].~TElement();
			}
		}
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class RingQueueTest : public TestCollection
	{
	public:
		RingQueueTest() : TestCollection("RingQueueTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
