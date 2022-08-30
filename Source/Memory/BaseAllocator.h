// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"
#include "AllocatorScope.h"
#include "MemoryManager.h"
#include "Config/EngineConfig.h"
#include <cstddef>
#include <iostream>


namespace HE
{
    template <class T>
    struct BaseAllocator final
    {
        using value_type = T;

        template <class U>
        using rebind = BaseAllocator<U>;
        
    private:
        TAllocatorID allocatorID;
        
    public:
        BaseAllocator()
            : allocatorID(MemoryManager::GetCurrentAllocatorID())
        {
        }
        
        T* allocate(std::size_t n)
        {
            AllocatorScope scope(allocatorID);
            auto& mmgr = MemoryManager::GetInstance();
            auto ptr = mmgr.Allocate<T>(n);
            
            return ptr;
        }

        void deallocate (T* ptr, std::size_t n)
        {
            AllocatorScope scope(allocatorID);
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.Deallocate(ptr, n);
        }
        
        template <class U>
        bool operator==(const BaseAllocator<U>& rhs) const
        {
            return allocatorID == rhs.allocatorID;
        }

        template <class U>
        bool operator!=(const BaseAllocator<U>& rhs) const
        {
            return allocatorID != rhs.allocatorID;
        }
        
        inline auto GetID() const { return allocatorID; }
        inline size_t GetFallbackCount() const { return 0; }
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
