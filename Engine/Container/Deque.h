// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>

#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"

namespace hbe
{

	template<typename TElement, class TAllocator = DefaultAllocator<TElement>>
	class Deque final
	{
	public:
		using TIndex = int;
		using Iterator = TElement*;
		using ConstIterator = const TElement*;

	private:
		static constexpr TIndex DefaultCapacity = 4;

		TAllocator allocator;
		TIndex head;
		TIndex tail;
		TIndex count;
		TIndex mask;
		TElement* data;

	public:
		Iterator begin() { return &data[head]; }
		Iterator end() { return &data[head + count]; }
		ConstIterator begin() const { return &data[head]; }
		ConstIterator end() const { return &data[head + count]; }

	public:
		Deque(const Deque&) = delete;
		Deque& operator=(const Deque&) = delete;

		Deque() noexcept
			: head(0)
			, tail(0)
			, count(0)
			, mask(DefaultCapacity - 1)
			, data(allocator.allocate(DefaultCapacity))
		{
		}

		explicit Deque(TIndex initialCapacity)
			: head(0)
			, tail(0)
			, count(0)
			, mask(DefaultCapacity - 1)
			, data(nullptr)
		{
			Reserve(initialCapacity > DefaultCapacity ? initialCapacity : DefaultCapacity);
		}

		Deque(Deque&& rhs) noexcept
			: Deque()
		{
			Swap(rhs);
		}

		~Deque()
		{
			if (data == nullptr)
				return;

			DestroyAll();
			allocator.deallocate(data, Capacity());
		}

		Deque& operator=(Deque&& rhs) noexcept
		{
			Swap(rhs);
			return *this;
		}

		TElement& operator[](TIndex index)
		{
			FatalAssert(IsValidIndex(index));
			return data[WrapIndex(head + index)];
		}

		const TElement& operator[](TIndex index) const
		{
			FatalAssert(IsValidIndex(index));
			return data[WrapIndex(head + index)];
		}

		void PushFront(const TElement& value)
		{
			if (count == Capacity())
			{
				Grow();
			}

			head = WrapIndex(head - 1);
			new (&data[head]) TElement(value);
			++count;
		}

		void PushFront(TElement&& value)
		{
			if (count == Capacity())
			{
				Grow();
			}

			head = WrapIndex(head - 1);
			new (&data[head]) TElement(std::move(value));
			++count;
		}

		template<typename... Types>
		TElement& EmplaceFront(Types&&... args)
		{
			if (count == Capacity())
			{
				Grow();
			}

			head = WrapIndex(head - 1);
			auto* ptr = new (&data[head]) TElement(std::forward<Types>(args)...);
			++count;
			return *ptr;
		}

		void PushBack(const TElement& value)
		{
			if (count == Capacity())
			{
				Grow();
			}

			new (&data[tail]) TElement(value);
			tail = WrapIndex(tail + 1);
			++count;
		}

		void PushBack(TElement&& value)
		{
			if (count == Capacity())
			{
				Grow();
			}

			new (&data[tail]) TElement(std::move(value));
			tail = WrapIndex(tail + 1);
			++count;
		}

		template<typename... Types>
		TElement& EmplaceBack(Types&&... args)
		{
			if (count == Capacity())
			{
				Grow();
			}

			auto* ptr = new (&data[tail]) TElement(std::forward<Types>(args)...);
			tail = WrapIndex(tail + 1);
			++count;
			return *ptr;
		}

		void PopFront()
		{
			FatalAssert(!IsEmpty());
			data[head].~TElement();
			head = WrapIndex(head + 1);
			--count;
		}

		void PopBack()
		{
			FatalAssert(!IsEmpty());
			tail = WrapIndex(tail - 1);
			data[tail].~TElement();
			--count;
		}

		TElement& Front()
		{
			FatalAssert(!IsEmpty());
			return data[head];
		}

		const TElement& Front() const
		{
			FatalAssert(!IsEmpty());
			return data[head];
		}

		TElement& Back()
		{
			FatalAssert(!IsEmpty());
			return data[WrapIndex(tail - 1)];
		}

		const TElement& Back() const
		{
			FatalAssert(!IsEmpty());
			return data[WrapIndex(tail - 1)];
		}

		[[nodiscard]] TIndex Size() const { return count; }
		[[nodiscard]] TIndex Capacity() const { return mask + 1; }
		[[nodiscard]] bool IsEmpty() const { return count == 0; }
		[[nodiscard]] bool IsValidIndex(TIndex index) const { return index >= 0 && index < count; }

		void Clear()
		{
			DestroyAll();
			head = 0;
			tail = 0;
			count = 0;
		}

		void Reserve(TIndex newCapacity)
		{
			if (newCapacity <= Capacity())
				return;

			auto newMask = Capacity() - 1;
			while (newMask + 1 < newCapacity)
			{
				newMask = (newMask + 1) * 2 - 1;
			}

			auto newSize = newMask + 1;
			auto* newData = allocator.allocate(newSize);

			for (TIndex i = 0; i < count; ++i)
			{
				auto srcIdx = WrapIndex(head + i);
				new (&newData[i]) TElement(std::move(data[srcIdx]));
				data[srcIdx].~TElement();
			}

			if (data != nullptr)
			{
				allocator.deallocate(data, Capacity());
			}

			data = newData;
			head = 0;
			tail = count;
			mask = static_cast<TIndex>(newMask);
		}

		void Swap(Deque& rhs) noexcept
		{
			std::swap(head, rhs.head);
			std::swap(tail, rhs.tail);
			std::swap(count, rhs.count);
			std::swap(mask, rhs.mask);
			std::swap(data, rhs.data);
		}

	private:
		TIndex WrapIndex(TIndex index) const { return index & mask; }

		void Grow()
		{
			auto newCapacity = std::max(DefaultCapacity, Capacity() * 2);
			Reserve(newCapacity);
		}

		void DestroyAll()
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

	class DequeTest : public TestCollection
	{
	public:
		DequeTest() : TestCollection("DequeTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
