// Created by mooming.go@gmail.com, 2017

#pragma once

namespace HE
{
    template <class T>
    struct BaseAllocator final
    {
        using value_type = T;
        T* allocate(std::size_t n);
        void deallocate (T* ptr, std::size_t n);
    };
} // HE
