// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "AllocatorScope.h"

#include "MemoryManager.h"


namespace hbe
{

	AllocatorScope::AllocatorScope() noexcept : AllocatorScope(InvalidAllocatorID) {}

	AllocatorScope::AllocatorScope(TAllocatorID id) noexcept
	{
		auto& mmgr = MemoryManager::getInstance();
		previous = mmgr.getScopedAllocatorID();
		current = id;

		mmgr.setScopedAllocatorID(current);
	}

	AllocatorScope::~AllocatorScope() noexcept
	{
		auto& mmgr = MemoryManager::getInstance();
		mmgr.setScopedAllocatorID(previous);
	}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "InlinePoolAllocator.h"

namespace hbe
{

	void AllocatorScopeTest::prepare()
	{
		addTest("Alloc Scope Test", [this](auto& ls)
		{
			InlinePoolAllocator<int, 100, 2> allocator;
			AllocatorScope scope(allocator);

			auto& mmgr = MemoryManager::getInstance();
			auto ptr = mmgr.allocate(100);

			if (mmgr.getCurrentAllocatorID() != allocator.getID())
			{
				ls << "Current allocator ID is not the given allocator id(" << allocator.getID() << ')' << lferr;
			}

			mmgr.Deallocate(ptr, 100);
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
