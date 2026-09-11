// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstring>
#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include "Config/EngineConfig.h"
#include "Core/Debug.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"
#include "String/StaticString.h"

namespace hbe
{

	/**
	 * @brief A fixed-size, stack-allocated memory pool allocator for type T.
	 *
	 * This allocator manages memory in pre-allocated blocks of a fixed size
	 * determined by {@link InlinePoolAllocator::ActualBlockSize}. It functions
	 * as an efficient replacement for general-purpose allocators when all
	 * allocated objects are of the same, known size (T).
	 *
	 * @tparam T The type of object to be allocated.
	 * @tparam BlockSize The desired size of the object allocated in bytes.
	 * @tparam NumBlocks The total number of blocks available in this pool.
	 *
	 * The internal memory pool is structured as a singly-linked free list,
	 * where the pointer to the next free block is stored at the beginning
	 * of each utilized block.
	 *
	 * Allocation prioritizes:
	 * 1. The immediate block (LIFO cache).
	 * 2. The managed free list.
	 * 3. Fallback to the global MemoryManager if the pool is exhausted.
	 *
	 * Deallocation returns the memory block to the free list or the
	 * immediate block, ensuring O(1) bookkeeping operations.
	 *
	 * Note: It interacts heavily with the global MemoryManager for fallback
	 * allocation and registration/de-registration of allocation IDs.
	 */
	template<class T, size_t BlockSize, size_t NumBlocks>
	class InlinePoolAllocator
	{
	public:
		using TIndex = size_t;
		using value_type = T;

		template<class TOther>
		struct rebind
		{
			using other = InlinePoolAllocator<TOther, BlockSize, NumBlocks>;
		};

		static constexpr size_t ActualBlockSize = std::max(BlockSize, sizeof(void*));
		static constexpr size_t ActualNumBlocks = std::max(NumBlocks, static_cast<size_t>(1));
		static constexpr size_t BlockSizeInBytes = sizeof(T) * ActualBlockSize;
		static constexpr auto AlignUnit = std::max(sizeof(T), Config::DefaultAlign);

	private:
		TAllocatorID id;
		TAllocatorID parentID;
		void* availableBlock;
		void* immediateBlock;
		alignas(AlignUnit) uint8_t block[ActualNumBlocks][BlockSizeInBytes];

	public:
		InlinePoolAllocator() :
			id(InvalidAllocatorID), parentID(InvalidAllocatorID), availableBlock(&block[0][0]), immediateBlock(nullptr)
		{
			Assert(OS::checkAligned(block[0]));
			parentID = MemoryManager::getCurrentAllocatorID();

			// Place a pointer to the next block at the beginning of blocks.
			for (size_t i = 1; i < ActualNumBlocks; i++)
			{
				writePointerToNext(block[i-1], block[i]);
			}

			writePointerToNext(block[ActualNumBlocks-1], nullptr);

#if MEMORY_VERIFICATION_ENABLED
			constexpr size_t length = sizeof(T) * ActualBlockSize * ActualNumBlocks;
			std::memset(buffer, 0, length);
#endif // MEMORY_VERIFICATION_ENABLED

			registerAllocator();
		}

		InlinePoolAllocator(const InlinePoolAllocator&) : InlinePoolAllocator() { Assert(false); }
		virtual ~InlinePoolAllocator() { deregisterAllocator(); }

		template<typename TOther>
		explicit operator InlinePoolAllocator<TOther, ActualBlockSize, ActualNumBlocks>() noexcept
		{
			using TCastedAlloc = InlinePoolAllocator<TOther, ActualBlockSize, ActualNumBlocks>;

			if (parentID != InvalidAllocatorID)
			{
				return TCastedAlloc(parentID);
			}

			return TCastedAlloc(getID());
		}

		[[nodiscard]] static StaticString getName()
		{
			static StaticString name("InlinePoolAllocator");
			return name;
		}

		// This function name is enforced by STL
		[[nodiscard]] T* allocate(std::size_t n) noexcept
		{
			const auto nBytes = n * sizeof(T);
			auto ptr = allocateBytes(nBytes);

			return static_cast<T*>(ptr);
		}

		// This function name is enforced by STL
		void deallocate(T* ptr, std::size_t n) noexcept
		{
			return deallocateBytes(ptr, n);
		}

		[[nodiscard]] auto getID() const { return id; }
		[[nodiscard]] static auto getBlockSize() { return ActualBlockSize; }
		[[nodiscard]] static auto getNumBlocks() { return ActualNumBlocks; }

		bool operator==(const InlinePoolAllocator&) const { return false; }
		bool operator!=(const InlinePoolAllocator&) const { return true; }

	private:
		void* allocateBytes(size_t nBytes) noexcept
		{
			if (nBytes <= BlockSizeInBytes)
			{
				if (immediateBlock != nullptr)
				{
					auto ptr = immediateBlock;
					immediateBlock = nullptr;

					return ptr;
				}

				if (availableBlock != nullptr)
				{
					T* ptr = static_cast<T*>(availableBlock);
					auto nextPtr = getPointerToNext(availableBlock);
					availableBlock = nextPtr;

#if PROFILE_ENABLED
					auto& mmgr = MemoryManager::getInstance();
					mmgr.reportAllocation(id, ptr, nBytes, BlockSizeInBytes);
#endif // PROFILE_ENABLED

					return ptr;
				}
			}

			auto& mmgr = MemoryManager::getInstance();
			auto ptr = mmgr.fallbackAllocate(getID(), parentID, nBytes);

			return ptr;
		}

		void deallocateBytes(void* ptr, size_t nBytes) noexcept
		{
			if (immediateBlock == nullptr && nBytes <= BlockSizeInBytes)
			{
				immediateBlock = ptr;
				return;
			}

			if (!isValidPointer(ptr))
			{
				// Fallback Deallocation
				auto& mmgr = MemoryManager::getInstance();
				mmgr.Deallocate(parentID, ptr, nBytes);
				return;
			}

			// Return a block
			writePointerToNext(ptr, availableBlock);
			availableBlock = ptr;

#if PROFILE_ENABLED
			auto& mmgr = MemoryManager::getInstance();
			mmgr.reportDeallocation(id, ptr, nBytes, BlockSizeInBytes);
#endif // PROFILE_ENABLED
		}

		void registerAllocator() noexcept
		{
			auto& mmgr = MemoryManager::getInstance();
			auto allocFunc = [](void* allocatorPtr, size_t nBytes) -> void*
			{
				auto allocator = static_cast<InlinePoolAllocator*>(allocatorPtr);
				return static_cast<void*>(allocator->allocateBytes(nBytes));
			};

			auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t nBytes)
			{
				auto allocator = static_cast<InlinePoolAllocator*>(allocatorPtr);
				allocator->deallocateBytes(ptr, nBytes);
			};

			const auto capacity = ActualBlockSize * ActualNumBlocks * sizeof(T);
			id = mmgr.registerAllocator(this, "InlinePoolAllocator", true, capacity, allocFunc, deallocFunc);

			fatalAssert(id != InvalidAllocatorID);
			fatalAssert(id != 0);
		}

		void deregisterAllocator() noexcept
		{
			if (id == InvalidAllocatorID)
			{
				return;
			}

			auto& mmgr = MemoryManager::getInstance();
			mmgr.deregisterAllocator(id);
			id = InvalidAllocatorID;
		}

		[[nodiscard]] bool isValidPointer(void* ptr) const
		{
			constexpr size_t LastBlockIndex = ActualNumBlocks - 1;
			return block[0] <= ptr && ptr <= block[LastBlockIndex];
		}

		static void writePointerToNext(void* ptr, void* nextPtr)
		{
			void** ptrArray = static_cast<void**>(ptr);
			auto& pointerToNext = reinterpret_cast<void*&>(ptrArray[0]);
			pointerToNext = nextPtr;
		}

		static void* getPointerToNext(void* ptr)
		{
			void** ptrArray = static_cast<void**>(ptr);
			auto& pointerToNext = reinterpret_cast<void*&>(ptrArray[0]);
			return pointerToNext;
		}
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	/// @brief Test class for InlinePoolAllocator.
	class InlinePoolAllocatorTest : public TestCollection
	{
	public:
		InlinePoolAllocatorTest() : TestCollection("InlinePoolAllocatorTest") {}

	protected:
		void prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
