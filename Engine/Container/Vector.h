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
		using TIndex = int;
		using Iterator = TElement*;
		using ConstIterator = const TElement*;

	private:
		static constexpr TIndex DefaultCapacity = 4;

		TAllocator allocator;
		TIndex count;
		TIndex cap;
		TElement* data;

	public:
		Iterator begin() { return data; }
		Iterator end() { return data + count; }
		ConstIterator begin() const { return data; }
		ConstIterator end() const { return data + count; }

	public:
		Vector(const Vector&) = delete;
		Vector& operator=(const Vector&) = delete;

		Vector() noexcept
			: count(0)
			, cap(0)
			, data(nullptr)
		{
		}

		explicit Vector(TIndex initialCapacity)
			: count(0)
			, cap(0)
			, data(nullptr)
		{
			Reserve(initialCapacity);
		}

		Vector(std::initializer_list<TElement> list)
			: count(0)
			, cap(0)
			, data(nullptr)
		{
			Reserve(static_cast<TIndex>(list.size()));
			for (auto& item : list)
			{
				PushBack(item);
			}
		}

		Vector(Vector&& rhs) noexcept
			: count(rhs.count)
			, cap(rhs.cap)
			, data(rhs.data)
		{
			rhs.count = 0;
			rhs.cap = 0;
			rhs.data = nullptr;
		}

		~Vector()
		{
			if (data == nullptr)
				return;

			DestroyAll();
			allocator.deallocate(data, cap);
		}

		Vector& operator=(Vector&& rhs) noexcept
		{
			if (this != &rhs)
			{
				if (data != nullptr)
				{
					DestroyAll();
					allocator.deallocate(data, cap);
				}

				count = rhs.count;
				cap = rhs.cap;
				data = rhs.data;

				rhs.count = 0;
				rhs.cap = 0;
				rhs.data = nullptr;
			}

			return *this;
		}

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

		void PushBack(const TElement& value)
		{
			if (count == cap)
			{
				Grow();
			}

			new (&data[count]) TElement(value);
			++count;
		}

		void PushBack(TElement&& value)
		{
			if (count == cap)
			{
				Grow();
			}

			new (&data[count]) TElement(std::move(value));
			++count;
		}

		template<typename... Types>
		TElement& EmplaceBack(Types&&... args)
		{
			if (count == cap)
			{
				Grow();
			}

			auto* ptr = new (&data[count]) TElement(std::forward<Types>(args)...);
			++count;

			return *ptr;
		}

		void PopBack()
		{
			FatalAssert(!IsEmpty());
			--count;
			data[count].~TElement();
		}

		void Resize(TIndex newSize)
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

		void Reserve(TIndex newCapacity)
		{
			if (newCapacity <= cap)
				return;

			auto* newData = allocator.allocate(newCapacity);

			for (TIndex i = 0; i < count; ++i)
			{
				new (&newData[i]) TElement(std::move(data[i]));
				data[i].~TElement();
			}

			if (data != nullptr)
			{
				allocator.deallocate(data, cap);
			}

			data = newData;
			cap = newCapacity;
		}

		void Clear()
		{
			DestroyAll();
			count = 0;
		}

		[[nodiscard]] TIndex Size() const { return count; }
		[[nodiscard]] TIndex Capacity() const { return cap; }
		[[nodiscard]] bool IsEmpty() const { return count == 0; }
		[[nodiscard]] bool IsValidIndex(TIndex index) const { return index >= 0 && index < count; }

		TElement* Data() { return data; }
		const TElement* Data() const { return data; }

		void Swap(Vector& rhs) noexcept
		{
			std::swap(count, rhs.count);
			std::swap(cap, rhs.cap);
			std::swap(data, rhs.data);
		}

		TIndex FindIndex(const TElement& element) const
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
		void Grow()
		{
			auto newCap = std::max(DefaultCapacity, cap * 2);
			Reserve(newCap);
		}

		void DestroyAll()
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
