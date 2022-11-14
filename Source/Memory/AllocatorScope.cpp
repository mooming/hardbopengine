// Created by mooming.go@gmail.com, 2022

#include "AllocatorScope.h"

#include "MemoryManager.h"


namespace HE
{

AllocatorScope::AllocatorScope() : AllocatorScope(InvalidAllocatorID)
{
}

AllocatorScope::AllocatorScope(TAllocatorID id)
{
    auto& mmgr = MemoryManager::GetInstance();
    previous = mmgr.GetScopedAllocatorID();
    current = id;

    mmgr.SetScopedAllocatorID(current);
}

AllocatorScope::~AllocatorScope()
{
    auto& mmgr = MemoryManager::GetInstance();
    mmgr.SetScopedAllocatorID(previous);
}

} // HE


#ifdef __UNIT_TEST__
#include "InlinePoolAllocator.h"


namespace HE
{

void AllocatorScopeTest::Prepare()
{
    AddTest("Alloc Scope Test", [this](auto& ls)
    {
        InlinePoolAllocator<int, 100> allocator;
        AllocatorScope scope(allocator);
        
        auto& mmgr = MemoryManager::GetInstance();
        auto ptr = mmgr.AllocateBytes(100);
        
        if (mmgr.GetCurrentAllocatorID() != allocator.GetID())
        {
            ls << "Current allocator ID is not the given allocator id("
            << allocator.GetID() << ')' << lferr;
        }

        mmgr.DeallocateBytes(ptr, 100);
    });
}

} // HE

#endif //__UNIT_TEST__
