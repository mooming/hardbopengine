// Created by mooming.go@gmail.com, 2022

#include "AllocatorScope.h"

#include "MemoryManager.h"


namespace HE
{

AllocatorScope::AllocatorScope(TAllocatorID id)
{
    auto& mmgr = MemoryManager::GetInstance();
    previous = mmgr.GetScopedAllocatorId();
    current = id;

    mmgr.SetScopedAllocatorId(current);
}

AllocatorScope::~AllocatorScope()
{
    auto& mmgr = MemoryManager::GetInstance();
    mmgr.SetScopedAllocatorId(previous);
}

} // HE


#ifdef __UNIT_TEST__

#include "InlineAllocator.h"
#include "InlinePoolAllocator.h"
#include <iostream>

namespace HE
{

bool AllocatorScopeTest::DoTest()
{
    using namespace std;

    

    cout << "AllocatorScope Tests :: DONE" << endl;

    return true;
}

} // HE

#endif //__UNIT_TEST__
