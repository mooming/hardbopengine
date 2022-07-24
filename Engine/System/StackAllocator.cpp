// Created by mooming.go@gmail.com, 2017

#include "System/StackAllocator.h"

#include "System/Debug.h"
#include "System/Exception.h"
#include "System/StdUtil.h"

using namespace HE;

StackAllocator::StackAllocator(size_t _capacity, AllocatorId allocatorId)
	: Allocator()
    , capacity(_capacity)
	, freeSize(_capacity)
	, cursor(0)
	, buffer(nullptr)
{
    FatalAssertMessage(_capacity > (sizeof(bool) + sizeof(SizeType))
        , "StackAllocator: Capacity is too small. Should be larger than %lu\n"
        , sizeof(bool) + sizeof(SizeType));
	bufferPtr = HE::Allocate(allocatorId, _capacity);
}

StackAllocator::~StackAllocator()
{
	HE::Deallocate(bufferPtr);
}

void *StackAllocator::Allocate (size_t size)
{
	// Structure
	// flag - 1 byte
	// bufer[N] - n byte
	// previousIndex - sizeType byte

    constexpr SizeType metaDataSize = sizeof(bool) + sizeof(SizeType);
	const size_t requiredSize = metaDataSize + size;

	if (freeSize >= requiredSize)
	{
		SizeType previous = cursor;

		SetAs<bool>(&buffer[cursor], true);
		cursor += sizeof(bool);
		Pointer ptr = &buffer[cursor];
		cursor += size;

		SetAs<SizeType>(&buffer[cursor], static_cast<SizeType>(previous));

		cursor += sizeof(SizeType);
		freeSize -= requiredSize;

		return ptr;
	}
	else
	{
		throw Exception(__FILE__, __LINE__,
			"StackAllocator: Not Enough Memory\n require = %u : free = %u / %u\n",
			size, freeSize, capacity);
	}

	return nullptr;
}

void StackAllocator::Deallocate(const Pointer ptr)
{
    Assert(IsMine(ptr));

	Byte* bytePtr = reinterpret_cast<Byte*>(ptr) - 1;
    AssertMessage(GetAs<bool>(bytePtr), "StackAllocator: Double Free Error\n");

	auto DoDealloc = [this, &bytePtr]() -> bool
	{
		AssertMessage(cursor <= capacity, "Invalid cursor = %lu, capacity = %lu\n", cursor, capacity);
		AssertMessage(cursor >= (sizeof(SizeType) + sizeof(bool)), "bad cursor = %lu, size type = %lu\n"
            , cursor, sizeof(SizeType));

        Pointer sizePtr = &buffer[cursor - sizeof(SizeType)];
		size_t previous = GetAs<SizeType>(sizePtr);
		AssertMessage(cursor > previous, "Invalid Cursor. Expecting cursor = %lu > previpus = %lu\n", cursor, previous);

		if (bytePtr == &buffer[previous])
		{
            const auto allocSize = cursor - previous;

			freeSize += allocSize;
			cursor = previous;

			if (cursor == 0)
			{
				return false;
			}
			else
			{
				bytePtr = &buffer[previous];
				return !GetAs<bool>(bytePtr);
			}
		}
		else
		{
			AssertMessage(GetAs<bool>(bytePtr), "try to free %p twice.", bytePtr + 1);
			SetAs<bool>(bytePtr, false);
  		}

		return false;
	};

	while (DoDealloc());
}

size_t StackAllocator::Available() const
{
	return freeSize;
}

size_t StackAllocator::Usage() const
{
	return capacity - freeSize;
}

bool StackAllocator::IsMine(Pointer ptr) const
{
    if (ptr < static_cast<void*>(buffer))
        return false;

    if (ptr >= static_cast<void*>(buffer + capacity))
        return false;

    return true;
}

#ifdef __UNIT_TEST__

#include <iostream>

#include "String.h"
#include "Vector.h"

bool StackAllocatorTest::DoTest()
{
    using namespace std;

    StackAllocator stack(1024 * 1024);
    AllocatorScope scope(stack.Id());

    {
        Vector<int> a;
        a.push_back(0);
    }

    if (stack.Usage() != 0)
    {
        cerr << "Deallocation Failed. Usage should be zero, but " << stack.Usage() << endl;
        return false;
    }

    {
        Vector<int> a;
        a.push_back(0);
        Vector<int> b;
        b.push_back(1);
    }

    if (stack.Usage() != 0)
    {
        cerr << "Deallocation Failed. Usage should be zero, but " << stack.Usage() << endl;
        return false;
    }

    {
        String a = "0";
    }

    if (stack.Usage() != 0)
    {
        cerr << "Deallocation Failed. Usage should be zero, but " << stack.Usage() << endl;
        return false;
    }

    {
        String a = "0";
        String b = "1";
    }

    if (stack.Usage() != 0)
    {
        cerr << "Deallocation Failed. Usage should be zero, but " << stack.Usage() << endl;
        return false;
    }

    return true;
}

#endif //__UNIT_TEST__
