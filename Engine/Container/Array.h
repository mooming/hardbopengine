// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <initializer_list>
#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"

namespace hbe
{

	// Static array supporting custom allocators
	/// @brief A dynamic array template supporting custom memory allocators
	template<typename Element, class TAllocator = DefaultAllocator<Element>>
	class Array final
	{
	public:
		using TIndex = int;
		using Iterator = Element*;
		using ConstIterator = const Element*;

	private:
		TIndex length;
		Element* data;
		TAllocator allocator;

	public:
		Iterator begin() { return &data[0]; }
		Iterator end() { return &data[length]; }
		ConstIterator begin() const { return &data[0]; }
		ConstIterator end() const { return &data[length]; }

	public:
		Array(const Array&) = delete;
		Array& operator=(const Array&) = delete;

	public:
		Array() noexcept : length(0), data(nullptr) {}

		explicit Array(TIndex size) : length(size)
		{
			data = allocator.allocate(length);
			for (TIndex i = 0; i < length; ++i)
			{
				new (&data[i]) Element();
			}
		}

		Array(std::initializer_list<Element> list) : Array(static_cast<TIndex>(list.size()))
		{
			TIndex index = 0;

			for (auto element : list)
			{
				data[index] = element;
				++index;
			}
		}

		Array(Array&& rhs) noexcept : Array() { Swap(rhs); }

		~Array()
		{
			if (data == nullptr)
			{
				return;
			}

			for (auto& item : *this)
			{
				item.~Element();
			}

			allocator.deallocate(data, length);
		}

		Array& operator=(Array&& rhs) noexcept
		{
			Swap(rhs);
			return *this;
		}

		Element& operator[](TIndex index) noexcept
		{
			FatalAssert(IsValidIndex(index));
			return data[index];
		}

		const Element& operator[](TIndex index) const noexcept
		{
			FatalAssert(IsValidIndex(index));
			return data[index];
		}

		template<typename... Types>
		Element& Emplace(TIndex index, Types&&... args) noexcept
		{
			FatalAssert(IsValidIndex(index));

			auto& item = data[index];
			item.~Element();

			new (&item) Element(std::forward<Types>(args)...);

			return item;
		}

		[[nodiscard]] Element* ToRawArray() noexcept { return data; }

		[[nodiscard]] const Element* const ToRawArray() const noexcept { return data; }

		[[nodiscard]] TIndex Size() const noexcept { return length; }

		[[nodiscard]] bool IsValidIndex(TIndex index) const noexcept { return index >= 0 && index < length; }

		void Clear() noexcept { Swap(Array()); }

		void Swap(Array&& rhs) noexcept
		{
			auto tmpLength = length;
			auto tmpData = data;

			length = rhs.length;
			data = rhs.data;

			rhs.length = tmpLength;
			rhs.data = tmpData;
		}

		TIndex GetIndex(const Element& element) const noexcept
		{
			if (unlikely(data == nullptr))
			{
				return -1;
			}

			auto delta = static_cast<TIndex>(&element - &data[0]);

			return IsValidIndex(delta) ? delta : -1;
		}
	};

} // namespace hbe

#ifdef __UNIT_TEST__

#include "Test/TestCollection.h"

namespace hbe
{

	class ArrayTest : public TestCollection
	{
	public:
		ArrayTest() : TestCollection("ArrayTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe

#endif //__UNIT_TEST__
