// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>

#include "Core/CommonMacros.h"
#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"


namespace hbe
{

	template<typename TElement, class TAllocator = DefaultAllocator<TElement>>
	class Deque final
	{
	public:
		static constexpr int DefaultCapacity = 4;

		using TIndex = int;
		using Iterator = TElement*;
		using ConstIterator = const TElement*;

		Deque() noexcept
			: head(0)
			, tail(0)
			, count(0)
			, mask(DefaultCapacity - 1)
			, data(allocator.allocate(DefaultCapacity))
		{
		}

		Deque(const Deque&) = delete;

		explicit Deque(TIndex initialCapacity)
			: head(0)
			, tail(0)
			, count(0)
			, mask(DefaultCapacity - 1)
			, data(nullptr)
		{
			reserve(initialCapacity > DefaultCapacity ? initialCapacity : DefaultCapacity);
		}

		Deque(Deque&& rhs) noexcept
			: Deque()
		{
			Swap(rhs);
		}

		~Deque()
		{
			returnIf(data == nullptr);

			destroyAll();
			allocator.deallocate(data, Capacity());
		}

		Deque& operator=(const Deque&) = delete;

		Deque& operator=(Deque&& rhs) noexcept
		{
			Swap(rhs);
			return *this;
		}

		Iterator begin() noexcept { return &data[head]; }
		Iterator end() noexcept { return &data[head + count]; }
		ConstIterator begin() const noexcept { return &data[head]; }
		ConstIterator end() const noexcept { return &data[head + count]; }

		TElement& operator[](TIndex index)
		{
			fatalAssert(isValidIndex(index));
			return data[wrapIndex(head + index)];
		}

		const TElement& operator[](TIndex index) const
		{
			fatalAssert(isValidIndex(index));
			return data[wrapIndex(head + index)];
		}

		void pushFront(const TElement& value) noexcept
		{
			if (count == Capacity())
			{
				grow();
			}

			head = wrapIndex(head - 1);
			new (&data[head]) TElement(value);
			++count;
		}

		void pushFront(TElement&& value) noexcept
		{
			if (count == Capacity())
			{
				grow();
			}

			head = wrapIndex(head - 1);
			new (&data[head]) TElement(std::move(value));
			++count;
		}

		template<typename... Types>
		TElement& emplaceFront(Types&&... args) noexcept
		{
			if (count == Capacity())
			{
				grow();
			}

			head = wrapIndex(head - 1);
			auto* ptr = new (&data[head]) TElement(std::forward<Types>(args)...);
			++count;
			return *ptr;
		}

		void pushBack(const TElement& value) noexcept
		{
			if (count == Capacity())
			{
				grow();
			}

			new (&data[tail]) TElement(value);
			tail = wrapIndex(tail + 1);
			++count;
		}

		void pushBack(TElement&& value) noexcept
		{
			if (count == Capacity())
			{
				grow();
			}

			new (&data[tail]) TElement(std::move(value));
			tail = wrapIndex(tail + 1);
			++count;
		}

		template<typename... Types>
		TElement& emplaceBack(Types&&... args) noexcept
		{
			if (count == Capacity())
			{
				grow();
			}

			auto* ptr = new (&data[tail]) TElement(std::forward<Types>(args)...);
			tail = wrapIndex(tail + 1);
			++count;
			return *ptr;
		}

		void popFront() noexcept
		{
			fatalAssert(!IsEmpty());
			data[head].~TElement();
			head = wrapIndex(head + 1);
			--count;
		}

		void popBack() noexcept
		{
			fatalAssert(!IsEmpty());
			tail = wrapIndex(tail - 1);
			data[tail].~TElement();
			--count;
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
		[[nodiscard]] TIndex Capacity() const noexcept { return mask + 1; }
		[[nodiscard]] bool IsEmpty() const noexcept { return count == 0; }
		[[nodiscard]] bool isValidIndex(TIndex index) const noexcept { return index >= 0 && index < count; }

		void clear() noexcept
		{
			destroyAll();
			head = 0;
			tail = 0;
			count = 0;
		}

		void reserve(TIndex newCapacity) noexcept
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
				auto srcIdx = wrapIndex(head + i);
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
		TAllocator allocator;
		TIndex head;
		TIndex tail;
		TIndex count;
		TIndex mask;
		TElement* data;

		TIndex wrapIndex(TIndex index) const noexcept { return index & mask; }

		void grow() noexcept
		{
			auto newCapacity = std::max(DefaultCapacity, Capacity() * 2);
			reserve(newCapacity);
		}

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

	class DequeTest : public TestCollection
	{
	public:
		DequeTest() : TestCollection("DequeTest") {}

	protected:
		void prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
