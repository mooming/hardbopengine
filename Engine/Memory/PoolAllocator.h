// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"
#include "Core/Types.h"
#include "OSAL/SourceLocation.h"
#include "String/StaticString.h"

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

#if PROFILE_ENABLED
		size_t maxUsedBlocks;
		hbe::source_location srcLocation;
#endif // PROFILE_ENABLED

	public:
#if PROFILE_ENABLED
		PoolAllocator(const char* name, TSize inBlockSize, TSize numberOfBlocks,
					  hbe::source_location location = hbe::source_location::current());
#else // PROFILE_ENABLED
		PoolAllocator(const char* name, TSize inBlockSize, TSize numberOfBlocks);
#endif // PROFILE_ENABLED

		PoolAllocator(PoolAllocator&& rhs) noexcept;
		~PoolAllocator();

		PoolAllocator& operator=(PoolAllocator&& rhs) noexcept;
		bool operator<(const PoolAllocator& rhs) const noexcept;

		Pointer Allocate(size_t size);
		void Deallocate(Pointer ptr, size_t size);
		bool IsMine(Pointer ptr) const;

		[[nodiscard]] auto GetID() const { return id; }
		[[nodiscard]] auto GetName() const { return name; }
		size_t GetSize(Pointer) const { return blockSize; }
		[[nodiscard]] size_t GetUsage() const { return (numberOfBlocks - numberOfFreeBlocks) * blockSize; }
		[[nodiscard]] TSize GetAvailableBlocks() const { return numberOfFreeBlocks; }
		[[nodiscard]] size_t GetAvailableMemory() const { return numberOfFreeBlocks * blockSize; }
		[[nodiscard]] size_t GetCapacity() const { return numberOfBlocks * blockSize; }
		[[nodiscard]] auto GetBlockSize() const { return blockSize; }
		[[nodiscard]] auto NumberOfFreeBlocks() const { return numberOfFreeBlocks; }
		[[nodiscard]] auto GetBuffer() const { return buffer; }

#if PROFILE_ENABLED
		[[nodiscard]] auto GetUsedBlocksMax() const { return maxUsedBlocks; }
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
		PoolAllocatorTest() : TestCollection("PoolAllocatorTest") {}

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
