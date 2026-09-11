// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdlib>
#include "Core/Debug.h"
#include "AllocatorID.h"
#include "AllocatorScope.h"
#include "MemoryManager.h"

namespace hbe
{

	// A proxy allocator which uses the current allocator defined in Memory Manager.
	// It should be careful to use this when the current allocator is stack allocators.
	template<typename T>
	class DefaultAllocator final
	{
	public:
		using value_type = T;

		template<class TOther>
		struct rebind
		{
			using other = DefaultAllocator<TOther>;
		};

	private:
		TAllocatorID allocatorID;

	public:
		DefaultAllocator() : allocatorID(MemoryManager::getCurrentAllocatorID()) {}

		template<class TOther>
		explicit DefaultAllocator(const DefaultAllocator<TOther>& rhs) noexcept : allocatorID(rhs.getSourceAllocatorID())
		{}

		[[nodiscard]] T* allocate(std::size_t n) noexcept
		{
			// Fast-path: when the scoped allocator is the SystemAllocator,
			// bypass the MemoryManager indirection chain and call malloc directly.
			// This eliminates multiple function calls per allocation that dominate
			// performance in hot paths (e.g., std::vector growth).
			if (allocatorID == MemoryManager::SystemAllocatorID)
			{
				return static_cast<T*>(malloc(n * sizeof(T)));
			}

			AllocatorScope scope(allocatorID);
			auto& mmgr = MemoryManager::getInstance();
			auto ptr = mmgr.allocateByType<T>(n);

			return ptr;
		}

		void deallocate(T* ptr, std::size_t n) noexcept
		{
			Assert(ptr != nullptr);

			// Fast-path: mirror the allocate() optimization for deallocation.
			if (allocatorID == MemoryManager::SystemAllocatorID)
			{
				free(ptr);
				return;
			}

			AllocatorScope scope(allocatorID);
			auto& mmgr = MemoryManager::getInstance();
			mmgr.deallocateTypes(ptr, n);
		}

		template<class TOther>
		bool operator==(const DefaultAllocator<TOther>& rhs) const noexcept
		{
			return allocatorID == rhs.allocatorID;
		}

		template<class TOther>
		bool operator!=(const DefaultAllocator<TOther>& rhs) const noexcept
		{
			return allocatorID != rhs.allocatorID;
		}

		[[nodiscard]] auto getID() const { return allocatorID; }
		[[nodiscard]] auto getSourceAllocatorID() const { return allocatorID; }
		[[nodiscard]] static constexpr size_t getFallbackCount() { return 0; }
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class BaseAllocatorTest : public TestCollection
	{
	public:
		BaseAllocatorTest() : TestCollection("BaseAllocatorTest") {}

	protected:
		void prepare() noexcept override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
