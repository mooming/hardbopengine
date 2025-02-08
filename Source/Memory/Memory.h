
// Created by mooming.go@gmail.com

#pragma once

#include "MemoryManager.h"
#include <cstddef>
#include <functional>

namespace HE
{

    using TAllocFunc = std::function<void *(size_t)>;
    using TDeallocFunc = std::function<void(void *, size_t)>;

    template <typename Type, typename... Types, typename TAllocator>
    Type *New(TAllocator &allocator, Types &&...args)
    {
        static_assert(sizeof(typename TAllocator::value_type) == sizeof(Type));

        auto ptr = allocator.allocate(1);
        auto tptr = new (ptr) Type(std::forward<Types>(args)...);

        return tptr;
    }

    template <typename Type, typename TAllocator>
    void Delete(TAllocator &allocator, Type *ptr)
    {
        static_assert(sizeof(typename TAllocator::value_type) == sizeof(Type));

        ptr->~Type();
        allocator.deallocate(ptr, 1);
    }

    template <typename Type, typename... Types>
    Type *New(Types &&...args)
    {
        auto &mmgr = MemoryManager::GetInstance();
        return mmgr.New<Type>(std::forward<Types>(args)...);
    }

    template <typename Type>
    void Delete(Type *ptr)
    {
        auto &mmgr = MemoryManager::GetInstance();
        mmgr.Delete<Type>(ptr);
    }

} // namespace HE
