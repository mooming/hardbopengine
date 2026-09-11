// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "AllocatorID.h"
#include "Core/Types.h"
#include "OSAL/SourceLocation.h"
#include "String/StaticString.h"

namespace hbe
{

	/// @brief Fixed-size block allocator for objects of a single type.
	/// @details Pre-allocates a pool of memory blocks and manages free list.
	/// Efficient for allocations of many objects of the same size.
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

#if PROFILE_ENABLED
		size_t maxUsedBlocks;
		hbe::source_location srcLocation;
#endif // PROFILE_ENABLED

	public:
#if PROFILE_ENABLED
		PoolAllocator(const char* inName, TSize inBlockSize, TSize inNumberOfBlocks,
					  hbe::source_location location = hbe::source_location::current());
#else // PROFILE_ENABLED
		PoolAllocator(const char* inName, TSize inBlockSize, TSize inNumberOfBlocks);
#endif // PROFILE_ENABLED

		PoolAllocator(PoolAllocator&& rhs) noexcept;
		~PoolAllocator();

		PoolAllocator& operator=(PoolAllocator&& rhs) noexcept;
		bool operator<(const PoolAllocator& rhs) const noexcept;

		Pointer allocate(size_t size);
		void Deallocate(Pointer ptr, size_t size);
		bool isMine(Pointer ptr) const;

		[[nodiscard]] auto getID() const { return id; }
		[[nodiscard]] auto getName() const { return name; }
		[[nodiscard]] size_t getSize(Pointer) const { return blockSize; }
		[[nodiscard]] size_t getUsage() const { return (numberOfBlocks - numberOfFreeBlocks) * blockSize; }
		[[nodiscard]] TSize getAvailableBlocks() const { return numberOfFreeBlocks; }
		[[nodiscard]] size_t getAvailableMemory() const { return numberOfFreeBlocks * blockSize; }
		[[nodiscard]] size_t getCapacity() const { return numberOfBlocks * blockSize; }
		[[nodiscard]] auto getBlockSize() const { return blockSize; }
		[[nodiscard]] auto NumberOfFreeBlocks() const { return numberOfFreeBlocks; }
		[[nodiscard]] auto getBuffer() const { return buffer; }

#if PROFILE_ENABLED
		[[nodiscard]] auto getUsedBlocksMax() const { return maxUsedBlocks; }
#endif // PROFILE_ENABLED

	private:
		TSize getIndex(Pointer ptr) const;
		TSize readNextIndex(Pointer ptr) const;
		void writeNextIndex(Pointer ptr, TSize index);

		Pointer allocateBlock();
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class PoolAllocatorTest : public TestCollection
	{
	public:
		PoolAllocatorTest() : TestCollection("PoolAllocatorTest") {}

	protected:
		void prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
