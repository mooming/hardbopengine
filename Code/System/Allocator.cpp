// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "System/System.h"

using namespace HE;

namespace
{
	constexpr auto SizeOfId = sizeof(AllocatorId);

	static MemoryManager& mmgr = MemoryManager::GetInstance();

	struct AllocationBlock
	{
		AllocatorId allocatorId;
		Byte data;

        inline AllocationBlock(AllocatorId allocId) : allocatorId(allocId) {}
		inline static AllocationBlock* FromBody(Pointer body)
		{
			Byte* ptr = reinterpret_cast<Byte*>(body);
			return reinterpret_cast<AllocationBlock*>(ptr - SizeOfId);
		}

		inline Pointer GetBody() { return &data; }
		inline Allocator& GetAllocator()
		{
			auto allocator = mmgr.GetAllocator(allocatorId);
			Assert(allocator != nullptr);

			return *allocator;
		}
	};
}

Allocator& HE::GetAllocator()
{
    auto allocator = mmgr.GetAllocator(mmgr.GetCurrent());
    Assert(allocator != nullptr);

    return *allocator;
}

Allocator& HE::GetAllocator(AllocatorId allocatorId)
{
    auto allocator = mmgr.GetAllocator(allocatorId);
    Assert(allocator != nullptr);

    return *allocator;
}

Pointer HE::Allocate(size_t size)
{
	return Allocate(mmgr.GetCurrent(), size);
}

Pointer HE::Allocate(AllocatorId allocatorId, size_t size)
{
	auto allocator = mmgr.GetAllocator(allocatorId);
	Assert(allocator != nullptr);

	const auto effectiveSive = SizeOfId + size;
	Pointer ptr = allocator->Allocate(effectiveSive);
    Assert(ptr != nullptr);

	AllocationBlock* allocBlock = Construct<AllocationBlock>(ptr, allocatorId);
	FatalAssertMessage(allocBlock != nullptr, "Memory Allocation Failed...");

	return allocBlock->GetBody();
}

void HE::Deallocate(const Pointer ptr)
{
	AssertMessage(ptr != nullptr, "Null pointer error. try to deallocate the null-pointer.");

	AllocationBlock* allocBlock = AllocationBlock::FromBody(ptr);
	Allocator& allocator = allocBlock->GetAllocator();

	allocator.Deallocate(reinterpret_cast<Pointer>(allocBlock));
}

size_t HE::GetSize(const Pointer ptr)
{
    AssertMessage(ptr != nullptr, "Null pointer error. Attempt to deallocate null-pointer.");

    AllocationBlock* allocBlock = AllocationBlock::FromBody(ptr);
    Allocator& allocator = allocBlock->GetAllocator();

    return allocator.GetSize(allocBlock);
}

Allocator::Allocator()
{
	id = mmgr.Register(this);
}

Allocator::Allocator(AllocatorId id) : id(id)
{
}

Allocator::~Allocator()
{
	mmgr.Deregister(id);
}

AllocatorScope::AllocatorScope(AllocatorId id)
{
    previous = mmgr.GetCurrent();
	mmgr.SetCurrent(id);
}

AllocatorScope::~AllocatorScope()
{
	mmgr.SetCurrent(previous);
}

#ifdef __UNIT_TEST__

#include <iostream>

bool AllocatorTest::DoTest()
{
    using namespace std;

    {
        AllocatorScope allocScope(1);

        auto freeSize = GetMemAvailable();
        auto ptr = Allocate(100);
        auto allocSize = GetSize(ptr);

        cout << "Alloc 100 bytes, allocated size = " << allocSize << endl;
        if (allocSize < 100)
        {
            cout << "Allocated size = " << allocSize << " is less than 100." << endl;
            return false;
        }

        auto freeSize2 = GetMemAvailable();
        if ((freeSize2 + allocSize) != freeSize)
        {
            cout << "Size Mismatch: "
                << freeSize << " - " << allocSize << " != " << freeSize2 << endl;

            return false;
        }

        Deallocate(ptr);
        auto freeSize3 = GetMemAvailable();

        if (freeSize3 != freeSize)
        {
            cout << "Memory Release Failed: "
                << freeSize << " -> " << freeSize2 << " -> " << freeSize3 << endl;

            return false;
        }
    }

    cout << "Allocator Tests :: DONE" << endl;

    return true;
}

#endif //__UNIT_TEST__
