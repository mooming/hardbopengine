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
/// API reference: docs/Container/FixedRingBuffer/index.html
template <typename TElement, class TAllocator = DefaultAllocator<TElement>>
class FixedRingBuffer final
{
public:
	using TIndex = int;

private:
	TIndex cap;
	TIndex head;
	TIndex tail;
	TIndex count;
	TElement* data;

public:
	FixedRingBuffer(const FixedRingBuffer&) = delete;

	explicit FixedRingBuffer(TIndex fixedCapacity)
		: cap(0)
		, head(0)
		, tail(0)
		, count(0)
		, data(nullptr)
	{
		FatalAssert(fixedCapacity > 0, "FixedRingBuffer capacity must be positive");

		TIndex pow2Capacity = static_cast<TIndex>(std::bit_ceil(static_cast<unsigned int>(fixedCapacity)));

		TAllocator alloc;
		data = alloc.allocate(pow2Capacity);
		cap = pow2Capacity;
	}

	FixedRingBuffer(FixedRingBuffer&& rhs) noexcept
		: cap(0)
		, head(0)
		, tail(0)
		, count(0)
		, data(nullptr)
	{
		Swap(rhs);
	}

	~FixedRingBuffer()
	{
		returnIf(data == nullptr);

		DestroyAll();
		TAllocator alloc;
		alloc.deallocate(data, cap);
	}

	FixedRingBuffer& operator=(const FixedRingBuffer&) = delete;

	FixedRingBuffer& operator=(FixedRingBuffer&& rhs) noexcept
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
		FatalAssert(!IsFull(), "FixedRingBuffer is full");
		new (&data[tail]) TElement(value);
		tail = WrapIndex(tail + 1);
		++count;
	}

	void Push(TElement&& value) noexcept
	{
		FatalAssert(!IsFull(), "FixedRingBuffer is full");
		new (&data[tail]) TElement(std::move(value));
		tail = WrapIndex(tail + 1);
		++count;
	}

	template <typename... Types>
	TElement& Emplace(Types&&... args) noexcept
	{
		FatalAssert(!IsFull(), "FixedRingBuffer is full");
		auto* ptr = new (&data[tail]) TElement(std::forward<Types>(args)...);
		tail = WrapIndex(tail + 1);
		++count;

		return *ptr;
	}

	TElement Pop() noexcept
	{
		FatalAssert(!IsEmpty(), "FixedRingBuffer is empty");
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

	[[nodiscard]] TIndex Size() const noexcept
	{
		return count;
	}

	[[nodiscard]] TIndex Capacity() const noexcept
	{
		return cap;
	}

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		return count == 0;
	}

	[[nodiscard]] bool IsFull() const noexcept
	{
		return count == cap;
	}

	[[nodiscard]] bool IsValidIndex(TIndex index) const noexcept
	{
		return index >= 0 && index < Size();
	}

	void Clear() noexcept
	{
		DestroyAll();
		head = 0;
		tail = 0;
		count = 0;
	}

	void Swap(FixedRingBuffer& rhs) noexcept
	{
		std::swap(cap, rhs.cap);
		std::swap(head, rhs.head);
		std::swap(tail, rhs.tail);
		std::swap(count, rhs.count);
		std::swap(data, rhs.data);
	}

	TIndex WrapIndex(TIndex index) const noexcept
	{
		return index & (cap - 1);
	}

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

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class FixedRingBufferTest : public TestCollection
{
public:
	FixedRingBufferTest()
		: TestCollection("FixedRingBufferTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif // TEST_ENABLED
