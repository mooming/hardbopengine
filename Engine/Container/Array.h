// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <initializer_list>
#include <utility>
#include "Core/CommonMacros.h"
#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"

namespace hbe
{

// Static array supporting custom allocators
/// @brief A dynamic array template supporting custom memory allocators
template <typename Element, class TAllocator = DefaultAllocator<Element>>
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
	Iterator begin()
	{
		return &data[0];
	}

	Iterator end()
	{
		return &data[length];
	}

	ConstIterator begin() const
	{
		return &data[0];
	}

	ConstIterator end() const
	{
		return &data[length];
	}

public:
	Array(const Array&) = delete;
	Array& operator=(const Array&) = delete;

public:
	Array() noexcept
		: length(0)
		, data(nullptr)
	{
	}

	explicit Array(TIndex size)
		: length(size)
	{
		data = allocator.allocate(length);
		for (TIndex i = 0; i < length; ++i)
		{
			new (&data[i]) Element();
		}
	}

	/// @brief Build an array of `size` elements drawing from a specific allocator instance.
	/// @details The other constructors default-construct the allocator, which for `DefaultAllocator` means
	///          capturing whatever allocator scope happens to be open at this moment - so the memory of the
	///          array is decided by where in the program the construction happened. This constructor states
	///          the allocator instead, so an array can be given an owner and stays in that owner's memory
	///          however it is reached later.
	/// @note The allocator is chosen before the first allocation and is what the destructor frees with, so
	///       allocating and freeing cannot end up split across two different pools.
	/// @note An allocator held here has to outlive the array. An adapter holding a reference to a pool
	///       inherits that pool's lifetime rule rather than a softer one.
	/// @note A non-positive size takes no memory at all, which is the state an array that has not been given
	///       its capacity lives in.
	Array(const TAllocator& inAllocator, TIndex size)
		: length(size)
		, data(nullptr)
		, allocator(inAllocator)
	{
		returnIf(length <= 0);

		data = allocator.allocate(length);
		for (TIndex i = 0; i < length; ++i)
		{
			new (&data[i]) Element();
		}
	}

	Array(std::initializer_list<Element> list)
		: Array(static_cast<TIndex>(list.size()))
	{
		TIndex index = 0;

		for (auto element : list)
		{
			data[index] = element;
			++index;
		}
	}

	Array(Array&& rhs) noexcept
		: Array()
	{
		Swap(rhs);
	}

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

	template <typename... Types>
	Element& Emplace(TIndex index, Types&&... args) noexcept
	{
		FatalAssert(IsValidIndex(index));

		auto& item = data[index];
		item.~Element();

		new (&item) Element(std::forward<Types>(args)...);

		return item;
	}

	[[nodiscard]] Element* ToRawArray() noexcept
	{
		return data;
	}

	[[nodiscard]] const Element* const ToRawArray() const noexcept
	{
		return data;
	}

	[[nodiscard]] TIndex Size() const noexcept
	{
		return length;
	}

	[[nodiscard]] bool IsValidIndex(TIndex index) const noexcept
	{
		return index >= 0 && index < length;
	}

	void Clear() noexcept
	{
		Swap(Array());
	}

	/// @brief Change the element count to exactly `newSize`, keeping the elements already present.
	/// @details The allocation is exact: the array ends up holding `newSize` elements, never a larger figure
	///          picked by a growth policy. A caller that has decided a ceiling therefore gets that ceiling and
	///          can keep reasoning with it, which is what a size chosen to fit a memory layout depends on.
	///          Growing keeps every element below the old `Size()` with its value intact and default-constructs
	///          the new tail; shrinking destroys the elements from `newSize` up and returns their memory.
	/// @note Every element is moved into the new buffer, so an element that is expensive to move, or one whose
	///       address someone else is holding, does not belong in an array that gets resized. A pointer into a
	///       resized array is a pointer into a freed buffer.
	/// @note Allocation failure leaves the array exactly as it was, at its old size with its old buffer, so a
	///       caller can ask again later. A resize to zero is not a failure: it empties the array and gives the
	///       buffer back.
	void Resize(TIndex newSize) noexcept
	{
		returnIf(newSize == length);

		auto newData = allocator.allocate(newSize);
		if (newData == nullptr && newSize > 0)
		{
			Assert(false, "Could not resize an array to", newSize, "elements; it keeps its old size of", length, ".");
			return;
		}

		auto oldData = data;
		auto oldLength = length;

		for (TIndex i = 0; i < newSize; ++i)
		{
			if (i < oldLength)
			{
				new (&newData[i]) Element(std::move(oldData[i]));
				continue;
			}

			new (&newData[i]) Element();
		}

		for (TIndex i = 0; i < oldLength; ++i)
		{
			oldData[i].~Element();
		}

		if (oldData != nullptr)
		{
			allocator.deallocate(oldData, oldLength);
		}

		data = newData;
		length = newSize;
	}

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
	ArrayTest()
		: TestCollection("ArrayTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe

#endif //__UNIT_TEST__
