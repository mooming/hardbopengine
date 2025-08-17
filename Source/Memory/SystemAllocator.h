// Created by mooming.go@gmail.com

#pragma once

#include "../Engine/Engine.h"
#include "Config/BuildConfig.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"
#include "System/Debug.h"
#include <cstddef>
#include <cstdlib>

namespace hbe
{
    template <class T>
    struct SystemAllocator
    {
    public:
        using value_type = T;

        template <class U>
        struct rebind
        {
            using other = SystemAllocator<U>;
        };

    public:
        SystemAllocator() = default;
        virtual ~SystemAllocator() = default;

#ifdef _MSC_VER
        template <class U>
        SystemAllocator(const SystemAllocator<U>&)
        {
        }
#endif // _MSC_VER

        constexpr auto GetID() const
        {
            return MemoryManager::SystemAllocatorID;
        }

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

        auto GetName() const { return "SystemAllocator"; }

    private:
        void* AllocateBytes(std::size_t nBytes)
        {
#ifdef __MEMORY_INVESTIGATION__
            if (unlikely(nBytes == 1))
            {
                nBytes = 1;
            }

            const auto pageSize = OS::GetPageSize();
            const size_t pageCount = (nBytes + pageSize - 1) / pageSize;
            const size_t allocSize = pageCount * pageSize;
            auto rawPtr = OS::VirtualAlloc(allocSize);

#ifdef __MEMORY_BUFFER_UNDERRUN_CHECK__
            T* ptr = reinterpret_cast<T*>(rawPtr);
#else  // __MEMORY_BUFFER_UNDERRUN_CHECK__
            auto bytePtr = static_cast<uint8_t*>(rawPtr);
            bytePtr = bytePtr + allocSize - nBytes;
            T* ptr = reinterpret_cast<T*>(bytePtr);
#endif // __MEMORY_BUFFER_UNDERRUN_CHECK__

#if __MEMORY_LOGGING__
            {
                auto& engine = Engine::Get();
                engine.Log(ELogLevel::Info, [this, rawPtr, ptr](auto& ls) {
                    ls << '[' << GetName()
                       << "][Investigator] RawPtr = " << rawPtr
                       << ", ptr = " << (void*)ptr;
                });
            }
#endif // __MEMORY_LOGGING__

#else  // __MEMORY_INVESTIGATION__
            auto ptr = (T*)malloc(nBytes);
#endif // __MEMORY_INVESTIGATION__

#ifdef PROFILE_ENABLED
            auto& mmgr = MemoryManager::GetInstance();

#ifdef __MEMORY_INVESTIGATION__
            const auto allocated = allocSize;
#else  // __MEMORY_INVESTIGATION__
            const auto allocated = OS::GetAllocSize(ptr);
#endif // __MEMORY_INVESTIGATION__

            mmgr.ReportAllocation(GetID(), ptr, nBytes, allocated);
#endif // PROFILE_ENABLED

            return ptr;
        }

        void DeallocateBytes(void* ptr, std::size_t nBytes)
        {
            Assert(ptr != nullptr, "[SysAlloc][Dealloc] Null Pointer Error");

#ifdef __MEMORY_INVESTIGATION__
            const auto pageSize = OS::GetPageSize();
            const size_t pageCount = (nBytes + pageSize - 1) / pageSize;
            const size_t allocSize = pageCount * pageSize;

#ifdef __MEMORY_BUFFER_UNDERRUN_CHECK__
            auto rawPtr = ptr;
#else  // __MEMORY_BUFFER_UNDERRUN_CHECK__
            auto bytePtr = reinterpret_cast<uint8_t*>(ptr);
            bytePtr = bytePtr + nBytes - allocSize;
            auto rawPtr = (void*)bytePtr;
#endif // __MEMORY_BUFFER_UNDERRUN_CHECK__
            const auto allocated = allocSize;
#endif // __MEMORY_INVESTIGATION__

#ifdef PROFILE_ENABLED

#ifndef __MEMORY_INVESTIGATION__
            auto allocated = OS::GetAllocSize(ptr);
#endif // __MEMORY_INVESTIGATION__

#ifdef __MEMORY_VERIFICATION__
            Assert(nBytes <= allocated);
#endif // __MEMORY_VERIFICATION__

            auto& mmgr = MemoryManager::GetInstance();
            mmgr.ReportDeallocation(GetID(), ptr, nBytes, allocated);
#endif // PROFILE_ENABLED

#ifdef __MEMORY_INVESTIGATION__

#ifdef __MEMORY_DANGLING_POINTER_CHECK__
            OS::ProtectMemory(rawPtr, allocated);
#else  // __MEMORY_DANGLING_POINTER_CHECK__
            OS::VirtualFree(rawPtr, allocated);
#endif // __MEMORY_DANGLING_POINTER_CHECK__

#else  // __MEMORY_INVESTIGATION__
            free(ptr);
#endif // __MEMORY_INVESTIGATION__
        }
    };
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
    class SystemAllocatorTest : public TestCollection
    {
    public:
        SystemAllocatorTest()
            : TestCollection("SystemAllocatorTest")
        {
        }

    protected:
        void Prepare() override;
    };
} // namespace hbe
#endif //__UNIT_TEST__
