// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <bit>
#include <cstddef>
#include <utility>

#include "Core/CommonMacros.h"
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
			fatalAssert(fixedCapacity > 0, "RingQueue capacity must be positive");

			// Round up to power of 2 for efficient bitmasking
			TIndex pow2Capacity = static_cast<TIndex>(std::bit_ceil(static_cast<unsigned int>(fixedCapacity)));

			TAllocator alloc;
			data = alloc.allocate(pow2Capacity);
			cap = pow2Capacity;
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
			returnIf(data == nullptr);

			destroyAll();
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
			fatalAssert(isValidIndex(index));
			return data[wrapIndex(head + index)];
		}

		const TElement& operator[](TIndex index) const noexcept
		{
			fatalAssert(isValidIndex(index));
			return data[wrapIndex(head + index)];
		}

		void push(const TElement& value) noexcept
		{
			fatalAssert(!isFull(), "RingQueue is full");
			new (&data[tail]) TElement(value);
			tail = wrapIndex(tail + 1);
			++count;
		}

		void push(TElement&& value) noexcept
		{
			fatalAssert(!isFull(), "RingQueue is full");
			new (&data[tail]) TElement(std::move(value));
			tail = wrapIndex(tail + 1);
			++count;
		}

		template<typename... Types>
		TElement& emplace(Types&&... args) noexcept
		{
			fatalAssert(!isFull(), "RingQueue is full");
			auto* ptr = new (&data[tail]) TElement(std::forward<Types>(args)...);
			tail = wrapIndex(tail + 1);
			++count;
			return *ptr;
		}

		TElement pop() noexcept
		{
			fatalAssert(!IsEmpty(), "RingQueue is empty");
			auto item = std::move(data[head]);
			data[head].~TElement();
			head = wrapIndex(head + 1);
			--count;

			return item;
		}

		TElement& front() noexcept
		{
			fatalAssert(!IsEmpty());
			return data[head];
		}

		const TElement& front() const noexcept
		{
			fatalAssert(!IsEmpty());
			return data[head];
		}

		TElement& back() noexcept
		{
			fatalAssert(!IsEmpty());
			return data[wrapIndex(tail - 1)];
		}

		const TElement& back() const noexcept
		{
			fatalAssert(!IsEmpty());
			return data[wrapIndex(tail - 1)];
		}

		[[nodiscard]] TIndex Size() const noexcept { return count; }
		[[nodiscard]] TIndex Capacity() const noexcept { return cap; }
		[[nodiscard]] bool IsEmpty() const noexcept { return count == 0; }
		[[nodiscard]] bool isFull() const noexcept { return count == cap; }
		[[nodiscard]] bool isValidIndex(TIndex index) const noexcept { return index >= 0 && index < Size(); }

		void clear() noexcept
		{
			destroyAll();
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

		TIndex wrapIndex(TIndex index) const noexcept { return index & (cap - 1); }

		void destroyAll() noexcept
		{
			for (TIndex i = 0; i < count; ++i)
			{
				auto idx = wrapIndex(head + i);
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
		void prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
