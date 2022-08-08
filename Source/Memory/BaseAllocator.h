// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Config/EngineConfig.h"
#include "Memory/MemoryManager.h"
#include <cstddef>
#include <iostream>


namespace HE
{
    template <class T>
    struct BaseAllocator final
    {
        using value_type = T;
        
        T* allocate(std::size_t n)
        {
            auto& mmgr = MemoryManager::GetInstance();
            auto ptr = mmgr.Allocate<T>(n);
            
            return ptr;
        }

        void deallocate (T* ptr, std::size_t n)
        {
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.Deallocate(ptr, n);
        }
        
        template <class U>
        bool operator==(const BaseAllocator<U>&) const
        {
            return true;
        }

        template <class U>
        bool operator!=(const BaseAllocator<U>&) const
        {
            return false;
        }
    };
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCase.h"

namespace HE
{
    class BaseAllocatorTest : public TestCase
    {
    public:

        BaseAllocatorTest() : TestCase("BaseAllocatorTest")
        {
        }

    protected:
        virtual bool DoTest() override;
    };
} // HE
#endif //__UNIT_TEST__
