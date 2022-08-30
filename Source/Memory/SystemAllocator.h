// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"
#include "MemoryManager.h"
#include "Config/EngineConfig.h"
#include "OSAL/Intrinsic.h"
#include "OSAL/OSMemory.h"
#include "System/Debug.h"
#include <cstddef>
#include <exception>
#include <iostream>


namespace HE
{
    template <class T>
    struct SystemAllocator final
    {
        using value_type = T;
        
    private:
        TAllocatorID id;
    
    public:
        SystemAllocator()
            : id(InvalidAllocatorID)
        {
            RegisterAllocator();
        }
        
        ~SystemAllocator()
        {
            DeregisterAllocator();
        }
        
        auto GetID() const { return id; }
        
        T* allocate(std::size_t n)
        {
            return reinterpret_cast<T*>(AllocateBytes(n * sizeof(T)));
        }

        void deallocate(T* ptr, std::size_t n)
        {
            DeallocateBytes(ptr, n * sizeof(T));
        }
        
        template <class U>
        bool operator==(const SystemAllocator<U>&)
        {
            return true;
        }

        template <class U>
        bool operator!=(const SystemAllocator<U>&)
        {
            return false;
        }
        
    private:
        void* AllocateBytes(std::size_t nBytes)
        {
#ifdef __MEMORY_INVESTIGATION__
            if (unlikely(nBytes == 1))
                nBytes = 1;
            
            const auto pageSize = OS::GetPageSize();
            const auto pageCount = (nBytes + pageSize - 1) / pageSize;
            const auto pageAllocSize = pageCount * pageSize;
            auto rawPtr = aligned_alloc(pageSize, pageAllocSize);
            uint8_t* bytePtr = (uint8_t*)rawPtr;
            
            T* ptr = nullptr;
            
            if (__MEMORY_OVERFLOW_CHECK__)
            {
                ptr = reinterpret_cast<T*>(bytePtr + pageAllocSize - nBytes);
            }
            else
            {
                ptr = reinterpret_cast<T*>(bytePtr);
            }

#else // __MEMORY_INVESTIGATION__
            auto ptr = (T*)malloc(nBytes);
#endif // __MEMORY_INVESTIGATION__
            
#ifdef __MEMORY_STATISTICS__
            auto& mmgr = MemoryManager::GetInstance();

#ifdef __MEMORY_INVESTIGATION__
            const auto allocated = OS::GetAllocSize(bytePtr);
#else // __MEMORY_INVESTIGATION__
            const auto allocated = OS::GetAllocSize(ptr);
#endif // __MEMORY_INVESTIGATION__
            
            mmgr.ReportAllocation(id, ptr, nBytes, allocated);
#endif // __MEMORY_STATISTICS__
            
            return ptr;
        }

        void DeallocateBytes(void* ptr, std::size_t nBytes)
        {
            Assert(ptr != nullptr, "[SysAlloc][Dealloc] Null Pointer Error");
            
#ifdef __MEMORY_INVESTIGATION__
            auto bytePtr = reinterpret_cast<uint8_t*>(ptr);
            
            if (__MEMORY_OVERFLOW_CHECK__)
            {
                const auto pageSize = OS::GetPageSize();
                const auto pageCount = (nBytes + pageSize - 1) / pageSize;
                const auto pageAllocSize = pageCount * pageSize;
                
                bytePtr = bytePtr + nBytes - pageAllocSize;
            }
            
            auto rawPtr = (void*)bytePtr;
            auto allocated = OS::GetAllocSize(rawPtr);
#endif // __MEMORY_INVESTIGATION__

#ifdef __MEMORY_STATISTICS__
#ifndef __MEMORY_INVESTIGATION__
            auto allocated = OS::GetAllocSize(ptr);
#endif // __MEMORY_INVESTIGATION__
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.ReportDeallocation(id, ptr, nBytes, allocated);
#endif // __MEMORY_STATISTICS__
            
#ifdef __MEMORY_INVESTIGATION__
            OS::ProtectMemory(rawPtr, allocated);
#else // __MEMORY_INVESTIGATION__
            free(ptr);
#endif // __MEMORY_INVESTIGATION__
        }
            
        void RegisterAllocator()
        {
            Assert(id == InvalidAllocatorID);
            
            auto& mmgr = MemoryManager::GetInstance();
            
            auto allocFunc = [this](size_t nBytes) -> void*
            {
                return static_cast<void*>(AllocateBytes(nBytes));
            };
            
            auto deallocFunc = [this](void* ptr, size_t nBytes)
            {
                DeallocateBytes(static_cast<T*>(ptr), nBytes);
            };
            
            id = mmgr.Register("SystemAllocator", false
                , 0, allocFunc, deallocFunc);
        }
        
        void DeregisterAllocator()
        {
            if (id == InvalidAllocatorID)
                return;
            
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.Deregister(id);
            
            id = InvalidAllocatorID;
        }
        
        friend class MemoryManager;
    };
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCase.h"

namespace HE
{
    class SystemAllocatorTest
        : public TestCase
    {
    public:

        SystemAllocatorTest()
            : TestCase("SystemAllocatorTest")
        {
        }

    protected:
        virtual bool DoTest() override;
    };
} // HE
#endif //__UNIT_TEST__
