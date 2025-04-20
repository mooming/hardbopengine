// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include "Config/EngineConfig.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"
#include "String/StaticString.h"
#include "System/Debug.h"
#include <cassert>
#include <cstddef>
#include <cstring>

namespace HE
{
    template <class T, int BufferSize, int NumBuffers = 2>
    struct InlinePoolAllocator
    {
        using TIndex = decltype(BufferSize);

        using value_type = T;

        template <class U>
        struct rebind
        {
            using other = InlinePoolAllocator<U, BufferSize, NumBuffers>;
        };

        static_assert(NumBuffers > 0,
            "The number of buffers should be greater than or equal to zero.");
        static_assert(std::is_signed<TIndex>(),
            "The type of BufferSize should be signed integral.");
        static_assert(BufferSize > 0,
            "The buffer size should be greater than or memsetequal to zero.");

    private:
        TAllocatorID id;
        TAllocatorID parentID;
        int indexHint;

        ALIGN bool isAllocated[NumBuffers];
        alignas(std::max(sizeof(T), Config::DefaultAlign))
            uint8_t buffer[NumBuffers][BufferSize * sizeof(T)];

    public:
        InlinePoolAllocator()
            : id(InvalidAllocatorID),
              parentID(InvalidAllocatorID),
              indexHint(0)
        {
            Assert(OS::CheckAligned(isAllocated));
            Assert(OS::CheckAligned(buffer[0]));

            auto& mmgr = MemoryManager::GetInstance();
            parentID = mmgr.GetCurrentAllocatorID();

            if (unlikely(BufferSize <= 0))
            {
                return;
            }

            for (auto& item : isAllocated)
            {
                item = false;
            }

#ifdef __MEMORY_VERIFICATION__
            constexpr size_t length = sizeof(T) * BufferSize * NumBuffers;
            std::memset(buffer, 0, length);
#endif // __MEMORY_VERIFICATION__

            RegisterAllocator();
        }

        InlinePoolAllocator(const InlinePoolAllocator&)
            : InlinePoolAllocator()
        {
            assert(false);
        }

        virtual ~InlinePoolAllocator() { DeregisterAllocator(); }

        template <typename U>
        operator InlinePoolAllocator<U, BufferSize, NumBuffers>()
        {
            using TCastedAlloc = InlinePoolAllocator<U, BufferSize, NumBuffers>;

            if (parentID != InvalidAllocatorID)
            {
                return TCastedAlloc(parentID);
            }

            return TCastedAlloc(GetID());
        }

        StaticString GetName() const
        {
            static StaticString name("InlinePoolAllocator");
            return name;
        }

        T* allocate(std::size_t n)
        {
            constexpr size_t unit = sizeof(T);
            const auto nBytes = OS::GetAligned(n * unit);
            auto ptr = AllocateBytes(nBytes);
            return reinterpret_cast<T*>(ptr);
        }

        void deallocate(T* ptr, std::size_t n) noexcept
        {
            constexpr size_t unit = sizeof(T);
            const auto nBytes = OS::GetAligned(n * unit);

            void* voidPtr = reinterpret_cast<void*>(ptr);
            return DeallocateBytes(voidPtr, nBytes);
        }

        auto GetID() const { return id; }
        auto GetBlockSize() const { return BufferSize; }
        auto GetNumBlocks() const { return NumBuffers; }

        bool operator==(const InlinePoolAllocator&) const { return false; }

        bool operator!=(const InlinePoolAllocator&) const { return true; }

    private:
        void* FallbackAlloc(size_t nBytes)
        {
            auto& mmgr = MemoryManager::GetInstance();
            auto ptr = mmgr.AllocateBytes(parentID, nBytes);

            return ptr;
        }

        void* AllocateBytes(size_t nBytes)
        {
            constexpr size_t unit = sizeof(T);
            constexpr size_t bufferSizeBytes = BufferSize * unit;

            if (nBytes > bufferSizeBytes)
            {
                return FallbackAlloc(nBytes);
            }

            if (unlikely(nBytes == 0))
            {
                return &buffer[0][0];
            }

            int index = indexHint;
            for (int i = 0; i < NumBuffers; ++i, ++index)
            {
                index = index >= NumBuffers ? 0 : index;
                if (isAllocated[index])
                {
                    continue;
                }

                indexHint = index + 1;
                if (indexHint >= NumBuffers)
                {
                    indexHint = 0;
                }

                isAllocated[index] = true;
                auto ptr = &buffer[index][0];

#ifdef PROFILE_ENABLED
                auto& mmgr = MemoryManager::GetInstance();
                mmgr.ReportAllocation(id, ptr, nBytes, bufferSizeBytes);
#endif // PROFILE_ENABLED

                return ptr;
            }

            return FallbackAlloc(nBytes);
        }

        void DeallocateBytes(void* ptr, size_t nBytes)
        {
            if (unlikely(ptr == nullptr || nBytes == 0))
            {
                return;
            }

            for (int i = 0; i < NumBuffers; ++i)
            {
                if (ptr != &buffer[i][0])
                {
                    continue;
                }

#ifdef PROFILE_ENABLED
                auto& mmgr = MemoryManager::GetInstance();
                constexpr size_t unit = sizeof(T);
                constexpr size_t bufferSizeBytes = BufferSize * unit;
                mmgr.ReportDeallocation(id, ptr, nBytes, bufferSizeBytes);
#endif // PROFILE_ENABLED

                isAllocated[i] = false;
                indexHint = i;

                return;
            }

            auto& mmgr = MemoryManager::GetInstance();
            mmgr.DeallocateBytes(parentID, ptr, nBytes);
        }

        void RegisterAllocator()
        {
            auto& mmgr = MemoryManager::GetInstance();

            auto allocFunc = [this](size_t nBytes) -> void* {
                return static_cast<void*>(AllocateBytes(nBytes));
            };

            auto deallocFunc = [this](void* ptr, size_t nBytes) {
                DeallocateBytes(ptr, nBytes);
            };

            const auto capacity = BufferSize * NumBuffers * sizeof(T);
            id = mmgr.Register(
                "InlinePoolAllocator", true, capacity, allocFunc, deallocFunc);

            FatalAssert(id != InvalidAllocatorID);
            FatalAssert(id != 0);
        }

        void DeregisterAllocator()
        {
            if (id == InvalidAllocatorID)
            {
                return;
            }

            auto& mmgr = MemoryManager::GetInstance();
            mmgr.Deregister(id);
        }
    };
} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

    class InlinePoolAllocatorTest : public TestCollection
    {
    public:
        InlinePoolAllocatorTest()
            : TestCollection("InlinePoolAllocatorTest")
        {
        }

    protected:
        virtual void Prepare() override;
    };

} // namespace HE
#endif //__UNIT_TEST__
