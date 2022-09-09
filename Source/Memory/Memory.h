
// Created by mooming.go@gmail.com, 2022

#pragma once

#include <concepts>
#include <cstddef>
#include <functional>


namespace HE
{

using TAllocFunc = std::function<void*(size_t)>;
using TDeallocFunc = std::function<void(void*, size_t)>;

template <typename Type, typename ... Types, typename TAllocator>
Type* New(TAllocator& allocator, Types&& ... args)
{
    auto ptr = allocator.allocate(1);
    auto tptr = new (ptr) Type(std::forward<Types>(args) ...);
    
    return tptr;
}

template <typename Type, typename TAllocator>
void Delete(TAllocator& allocator, Type* ptr)
{
    ptr->~Type();
    
    allocator.deallocate(ptr, 1);
}

} // HE
