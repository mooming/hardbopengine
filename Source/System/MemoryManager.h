// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef MemoryManager_h
#define MemoryManager_h

#include "Types.h"

#include <cstdint>

namespace HE
{
    class Allocator;

    class MemoryManager
    {
    private:
        Allocator* allocators[0xFF];
        AllocatorId freeId;
        AllocatorId currentId;

    public:
        MemoryManager(const MemoryManager&) = delete;
        MemoryManager& operator= (const MemoryManager&) = delete;

    protected:
        MemoryManager();
        ~MemoryManager() = default;

    public:
        void SetAllocator(AllocatorId id);
        AllocatorId GetAllocator();
        size_t TotalUsage();

    public:
        inline static MemoryManager& GetInstance()
        {
            static MemoryManager instance;
            return instance;
        }

        inline AllocatorId GetCurrent()
        {
            return currentId;
        }

        inline void SetCurrent(AllocatorId id)
        {
            currentId = id;
        }

        inline Allocator* GetAllocator(AllocatorId id)
        {
            return allocators[id];
        }

        inline AllocatorId Register(Allocator* allocator)
        {
            auto id = IssueId();
            allocators[id] = allocator;
            return id;
        }

        inline void Deregister(AllocatorId id)
        {
            allocators[id] = reinterpret_cast<Allocator*>(freeId);
            freeId = id;
        }

    private:
        inline AllocatorId IssueId()
        {
            AllocatorId issuedId = freeId;
            freeId = static_cast<AllocatorId>(reinterpret_cast<std::uintptr_t>(allocators[freeId]));

            return issuedId;
        }
    };
}

#endif /* MemoryManager_h */
