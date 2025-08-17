// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"
#include "OSAL/SourceLocation.h"
#include "String/StaticString.h"
#include "System/Types.h"

namespace hbe
{

    class PoolAllocator final
    {
        using TSize = size_t;

    private:
        TAllocatorID id;
        TAllocatorID parentID;
        StaticString name;

        TSize blockSize;
        TSize numberOfBlocks;
        TSize numberOfFreeBlocks;

        Byte* buffer;
        Pointer availables;

#ifdef PROFILE_ENABLED
        size_t maxUsedBlocks;
        std::source_location srcLocation;
#endif // PROFILE_ENABLED

    public:
#ifdef PROFILE_ENABLED
        PoolAllocator(const char* name, TSize inBlockSize, TSize numberOfBlocks,
            const std::source_location location =
                std::source_location::current());
#else  // PROFILE_ENABLED
        PoolAllocator(
            const char* name, TSize inBlockSize, TIndex numberOfBlocks);
#endif // PROFILE_ENABLED

        PoolAllocator(PoolAllocator&& rhs) noexcept;
        ~PoolAllocator();

        PoolAllocator& operator=(PoolAllocator&& rhs) noexcept;
        bool operator<(const PoolAllocator& rhs) const noexcept;

        Pointer Allocate(size_t size);
        void Deallocate(Pointer ptr, size_t size);
        bool IsMine(Pointer ptr) const;

        inline auto GetID() const { return id; }
        inline auto GetName() const { return name; }
        inline size_t GetSize(const Pointer) const { return blockSize; }
        inline size_t GetUsage() const
        {
            return (numberOfBlocks - numberOfFreeBlocks) * blockSize;
        }
        inline TSize GetAvailableBlocks() const { return numberOfFreeBlocks; }
        inline size_t GetAvailableMemory() const
        {
            return numberOfFreeBlocks * blockSize;
        }
        inline size_t GetCapacity() const { return numberOfBlocks * blockSize; }
        inline auto GetBlockSize() const { return blockSize; }
        inline auto NumberOfFreeBlocks() const { return numberOfFreeBlocks; }

#ifdef PROFILE_ENABLED
        inline auto GetUsedBlocksMax() const { return maxUsedBlocks; }
#endif // PROFILE_ENABLED

    private:
        TSize GetIndex(Pointer ptr) const;
        TSize ReadNextIndex(Pointer ptr) const;
        void WriteNextIndex(Pointer ptr, TSize index);

        Pointer AllocateBlock();
    };

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
    class PoolAllocatorTest : public TestCollection
    {
    public:
        PoolAllocatorTest()
            : TestCollection("PoolAllocatorTest")
        {
        }

    protected:
        virtual void Prepare() override;
    };
} // namespace hbe
#endif //__UNIT_TEST__
