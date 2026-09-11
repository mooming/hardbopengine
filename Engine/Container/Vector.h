// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "Core/CommonMacros.h"
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
			reserve(initialCapacity);
		}

		Vector(std::initializer_list<TElement> list)
			: count(0)
			, capacity(0)
			, data(nullptr)
		{
			reserve(static_cast<TIndex>(list.size()));
			for (auto& item : list)
			{
				pushBack(item);
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
			returnIf(data == nullptr);

			destroyAll();
			allocator.deallocate(data, capacity);
		}

		Vector& operator=(const Vector&) = delete;

		Vector& operator=(Vector&& rhs) noexcept
		{
			if (this != &rhs)
			{
				if (data != nullptr)
				{
					destroyAll();
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
			fatalAssert(isValidIndex(index));
			return data[index];
		}

		const TElement& operator[](TIndex index) const
		{
			fatalAssert(isValidIndex(index));
			return data[index];
		}

		TElement& front()
		{
			fatalAssert(!IsEmpty());
			return data[0];
		}

		const TElement& front() const
		{
			fatalAssert(!IsEmpty());
			return data[0];
		}

		TElement& back()
		{
			fatalAssert(!IsEmpty());
			return data[count - 1];
		}

		const TElement& back() const
		{
			fatalAssert(!IsEmpty());
			return data[count - 1];
		}

		void pushBack(const TElement& value) noexcept
		{
			if (count == capacity)
			{
				grow();
			}

			new (&data[count]) TElement(value);
			++count;
		}

		void pushBack(TElement&& value) noexcept
		{
			if (count == capacity)
			{
				grow();
			}

			new (&data[count]) TElement(std::move(value));
			++count;
		}

		template<typename... Types>
		TElement& emplaceBack(Types&&... args) noexcept
		{
			if (count == capacity)
			{
				grow();
			}

			auto* ptr = new (&data[count]) TElement(std::forward<Types>(args)...);
			++count;

			return *ptr;
		}

		void popBack() noexcept
		{
			fatalAssert(!IsEmpty());
			--count;
			data[count].~TElement();
		}

		void resize(TIndex newSize) noexcept
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
				reserve(newSize);
				for (TIndex i = count; i < newSize; ++i)
				{
					new (&data[i]) TElement();
				}
			}

			count = newSize;
		}

		void reserve(TIndex newCapacity) noexcept
		{
			returnIf(newCapacity <= capacity);

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

		void clear() noexcept
		{
			destroyAll();
			count = 0;
		}

		[[nodiscard]] TIndex Size() const noexcept { return count; }
		[[nodiscard]] TIndex Capacity() const noexcept { return capacity; }
		[[nodiscard]] bool IsEmpty() const noexcept { return count == 0; }
		[[nodiscard]] bool isValidIndex(TIndex index) const noexcept { return index >= 0 && index < count; }

		[[nodiscard]] TElement* Data() noexcept { return data; }
		[[nodiscard]] const TElement* Data() const noexcept { return data; }

		void Swap(Vector& rhs) noexcept
		{
			std::swap(count, rhs.count);
			std::swap(capacity, rhs.capacity);
			std::swap(data, rhs.data);
		}

		[[nodiscard]] TIndex findIndex(const TElement& element) const noexcept
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

		void grow() noexcept
		{
			auto newCap = std::max(DefaultCapacity, capacity * 2);
			reserve(newCap);
		}

		void destroyAll() noexcept
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
		void prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
