// Created by mooming.go@gmail.com

#pragma once

#include <cstddef>
#include "AllocatorID.h"
#include "AllocatorScope.h"
#include "MemoryManager.h"

namespace hbe
{

	// A proxy allocator which uses the current allocator defined in Memory Manager.
	// It should be careful to use this when the current allocator is stack allocators.
	template<typename T>
	class DefaultAllocator
	{
	public:
		using value_type = T;

		template<class U>
		struct rebind
		{
			using other = DefaultAllocator<U>;
		};

	private:
		TAllocatorID allocatorID;

	public:
		DefaultAllocator() : allocatorID(MemoryManager::GetCurrentAllocatorID()) {}

		template<class U>
		explicit DefaultAllocator(const DefaultAllocator<U>& rhs) : allocatorID(rhs.GetSourceAllocatorID())
		{}

		T* allocate(std::size_t n)
		{
			AllocatorScope scope(allocatorID);
			auto& mmgr = MemoryManager::GetInstance();
			auto ptr = mmgr.AllocateByType<T>(n);

			return ptr;
		}

		void deallocate(T* ptr, std::size_t n)
		{
			AllocatorScope scope(allocatorID);
			auto& mmgr = MemoryManager::GetInstance();
			mmgr.DeallocateTypes(ptr, n);
		}

		template<class U>
		bool operator==(const DefaultAllocator<U>& rhs) const
		{
			return allocatorID == rhs.allocatorID;
		}

		template<class U>
		bool operator!=(const DefaultAllocator<U>& rhs) const
		{
			return allocatorID != rhs.allocatorID;
		}

		auto GetID() const { return allocatorID; }
		auto GetSourceAllocatorID() const { return allocatorID; }
		static constexpr size_t GetFallbackCount() { return 0; }
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
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
