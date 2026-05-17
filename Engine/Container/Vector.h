// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"


namespace hbe
{

	template<typename TElement, class TAllocator = DefaultAllocator<TElement>>
	class Vector final
	{
	public:
		static constexpr int DefaultCapacity = 4;

		using TIndex = int;
		using Iterator = TElement*;
		using ConstIterator = const TElement*;

		Vector() noexcept
			: count(0)
			, capacity(0)
			, data(nullptr)
		{
		}

		explicit Vector(TIndex initialCapacity)
			: count(0)
			, capacity(0)
			, data(nullptr)
		{
			Reserve(initialCapacity);
		}

		Vector(std::initializer_list<TElement> list)
			: count(0)
			, capacity(0)
			, data(nullptr)
		{
			Reserve(static_cast<TIndex>(list.size()));
			for (auto& item : list)
			{
				PushBack(item);
			}
		}

		Vector(const Vector&) = delete;

		Vector(Vector&& rhs) noexcept
			: count(rhs.count)
			, capacity(rhs.capacity)
			, data(rhs.data)
		{
			rhs.count = 0;
			rhs.capacity = 0;
			rhs.data = nullptr;
		}

		~Vector()
		{
			if (data == nullptr)
				return;

			DestroyAll();
			allocator.deallocate(data, capacity);
		}

		Vector& operator=(const Vector&) = delete;

		Vector& operator=(Vector&& rhs) noexcept
		{
			if (this != &rhs)
			{
				if (data != nullptr)
				{
					DestroyAll();
					allocator.deallocate(data, capacity);
				}

				count = rhs.count;
				capacity = rhs.capacity;
				data = rhs.data;

				rhs.count = 0;
				rhs.capacity = 0;
				rhs.data = nullptr;
			}

			return *this;
		}

		Iterator begin() noexcept { return data; }
		Iterator end() noexcept { return data + count; }
		ConstIterator begin() const noexcept { return data; }
		ConstIterator end() const noexcept { return data + count; }

		TElement& operator[](TIndex index)
		{
			FatalAssert(IsValidIndex(index));
			return data[index];
		}

		const TElement& operator[](TIndex index) const
		{
			FatalAssert(IsValidIndex(index));
			return data[index];
		}

		TElement& Front()
		{
			FatalAssert(!IsEmpty());
			return data[0];
		}

		const TElement& Front() const
		{
			FatalAssert(!IsEmpty());
			return data[0];
		}

		TElement& Back()
		{
			FatalAssert(!IsEmpty());
			return data[count - 1];
		}

		const TElement& Back() const
		{
			FatalAssert(!IsEmpty());
			return data[count - 1];
		}

		void PushBack(const TElement& value) noexcept
		{
			if (count == capacity)
			{
				Grow();
			}

			new (&data[count]) TElement(value);
			++count;
		}

		void PushBack(TElement&& value) noexcept
		{
			if (count == capacity)
			{
				Grow();
			}

			new (&data[count]) TElement(std::move(value));
			++count;
		}

		template<typename... Types>
		TElement& EmplaceBack(Types&&... args) noexcept
		{
			if (count == capacity)
			{
				Grow();
			}

			auto* ptr = new (&data[count]) TElement(std::forward<Types>(args)...);
			++count;

			return *ptr;
		}

		void PopBack() noexcept
		{
			FatalAssert(!IsEmpty());
			--count;
			data[count].~TElement();
		}

		void Resize(TIndex newSize) noexcept
		{
			if (newSize < count)
			{
				for (TIndex i = newSize; i < count; ++i)
				{
					data[i].~TElement();
				}
			}
			else if (newSize > count)
			{
				Reserve(newSize);
				for (TIndex i = count; i < newSize; ++i)
				{
					new (&data[i]) TElement();
				}
			}

			count = newSize;
		}

		void Reserve(TIndex newCapacity) noexcept
		{
			if (newCapacity <= capacity)
				return;

			auto* newData = allocator.allocate(newCapacity);

			for (TIndex i = 0; i < count; ++i)
			{
				new (&newData[i]) TElement(std::move(data[i]));
				data[i].~TElement();
			}

			if (data != nullptr)
			{
				allocator.deallocate(data, capacity);
			}

			data = newData;
			capacity = newCapacity;
		}

		void Clear() noexcept
		{
			DestroyAll();
			count = 0;
		}

		[[nodiscard]] TIndex Size() const noexcept { return count; }
		[[nodiscard]] TIndex Capacity() const noexcept { return capacity; }
		[[nodiscard]] bool IsEmpty() const noexcept { return count == 0; }
		[[nodiscard]] bool IsValidIndex(TIndex index) const noexcept { return index >= 0 && index < count; }

		[[nodiscard]] TElement* Data() noexcept { return data; }
		[[nodiscard]] const TElement* Data() const noexcept { return data; }

		void Swap(Vector& rhs) noexcept
		{
			std::swap(count, rhs.count);
			std::swap(capacity, rhs.capacity);
			std::swap(data, rhs.data);
		}

		[[nodiscard]] TIndex FindIndex(const TElement& element) const noexcept
		{
			for (TIndex i = 0; i < count; ++i)
			{
				if (data[i] == element)
				{
					return i;
				}
			}

			return -1;
		}

	private:
		TAllocator allocator;
		TIndex count;
		TIndex capacity;
		TElement* data;

		void Grow() noexcept
		{
			auto newCap = std::max(DefaultCapacity, capacity * 2);
			Reserve(newCap);
		}

		void DestroyAll() noexcept
		{
			for (TIndex i = 0; i < count; ++i)
			{
				data[i].~TElement();
			}
		}
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class VectorTest : public TestCollection
	{
	public:
		VectorTest() : TestCollection("VectorTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
