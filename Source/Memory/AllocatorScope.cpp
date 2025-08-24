// Created by mooming.go@gmail.com

#include "AllocatorScope.h"

#include "MemoryManager.h"

namespace hbe
{

	AllocatorScope::AllocatorScope() : AllocatorScope(InvalidAllocatorID) {}

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

} // namespace hbe

#ifdef __UNIT_TEST__
#include "InlinePoolAllocator.h"

namespace hbe
{

	void AllocatorScopeTest::Prepare()
	{
		AddTest("Alloc Scope Test", [this](auto& ls)
		{
			InlinePoolAllocator<int, 100> allocator;
			AllocatorScope scope(allocator);

			auto& mmgr = MemoryManager::GetInstance();
			auto ptr = mmgr.Allocate(100);

			if (mmgr.GetCurrentAllocatorID() != allocator.GetID())
			{
				ls << "Current allocator ID is not the given allocator id(" << allocator.GetID() << ')' << lferr;
			}

			mmgr.Deallocate(ptr, 100);
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
