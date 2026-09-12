

#pragma once

#include <cstddef>
#include <functional>
#include "MemoryManager.h"

namespace hbe
{

	using TAllocFunc = std::function<void*(size_t)>;
	using TDeallocFunc = std::function<void(void*, size_t)>;

	template<typename TType, typename... TTypes, typename TAllocator>
	TType* New(TAllocator& allocator, TTypes&&... args) noexcept
	{
		static_assert(sizeof(typename TAllocator::value_type) == sizeof(TType));

		auto ptr = allocator.allocate(1);
		auto tptr = new (ptr) TType(std::forward<TTypes>(args)...);

		return tptr;
	}

	template<typename TType, typename TAllocator>
	void Delete(TAllocator& allocator, TType* ptr) noexcept
	{
		static_assert(sizeof(typename TAllocator::value_type) == sizeof(TType));

		ptr->~TType();
		allocator.deallocate(ptr, 1);
	}

	template<typename TType, typename... TTypes>
	TType* New(TTypes&&... args) noexcept
	{
		auto& mmgr = MemoryManager::GetInstance();
		return mmgr.New<TType>(std::forward<TTypes>(args)...);
	}

	template<typename TType>
	void Delete(TType* ptr) noexcept
	{
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.Delete<TType>(ptr);
	}

} // namespace hbe
