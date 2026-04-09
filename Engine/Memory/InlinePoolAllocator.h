// Created by mooming.go@gmail.com

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
	struct InlinePoolAllocator
	{
		using TIndex = size_t;
		using value_type = T;

		template<class U>
		struct rebind
		{
			using other = InlinePoolAllocator<U, BlockSize, NumBlocks>;
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
		InlinePoolAllocator() : id(InvalidAllocatorID), parentID(InvalidAllocatorID), availableBlock(&block[0][0]), immediateBlock(nullptr)
		{
			Assert(OS::CheckAligned(block[0]));
			parentID = MemoryManager::GetCurrentAllocatorID();

			// Place a pointer to the next block at the beginning of blocks.
			for (size_t i = 1; i < ActualNumBlocks; i++)
			{
				WritePointerToNext(block[i-1], block[i]);
			}

			WritePointerToNext(block[ActualNumBlocks-1], nullptr);

#if __MEMORY_VERIFICATION__
			constexpr size_t length = sizeof(T) * ActualBlockSize * ActualNumBlocks;
			std::memset(buffer, 0, length);
#endif // __MEMORY_VERIFICATION__

			RegisterAllocator();
		}

		InlinePoolAllocator(const InlinePoolAllocator&) : InlinePoolAllocator() { Assert(false); }
		virtual ~InlinePoolAllocator() { DeregisterAllocator(); }

		template<typename U>
		explicit operator InlinePoolAllocator<U, ActualBlockSize, ActualNumBlocks>()
		{
			using TCastedAlloc = InlinePoolAllocator<U, ActualBlockSize, ActualNumBlocks>;

			if (parentID != InvalidAllocatorID)
			{
				return TCastedAlloc(parentID);
			}

			return TCastedAlloc(GetID());
		}

		[[nodiscard]] static StaticString GetName()
		{
			static StaticString name("InlinePoolAllocator");
			return name;
		}

		// This function name is enforced by STL
		T* allocate(std::size_t n)
		{
			const auto nBytes = n * sizeof(T);
			auto ptr = AllocateBytes(nBytes);

			return static_cast<T*>(ptr);
		}

		// This function name is enforced by STL
		void deallocate(T* ptr, std::size_t n) noexcept
		{
			return DeallocateBytes(ptr, n);
		}

		auto GetID() const { return id; }
		static auto GetBlockSize() { return ActualBlockSize; }
		static auto GetNumBlocks() { return ActualNumBlocks; }

		bool operator==(const InlinePoolAllocator&) const { return false; }
		bool operator!=(const InlinePoolAllocator&) const { return true; }

	private:
		void* AllocateBytes(size_t nBytes)
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
					auto nextPtr = GetPointerToNext(availableBlock);
					availableBlock = nextPtr;

#if PROFILE_ENABLED
					auto& mmgr = MemoryManager::GetInstance();
					mmgr.ReportAllocation(id, ptr, nBytes, BlockSizeInBytes);
#endif // PROFILE_ENABLED

					return ptr;
				}
			}

			auto& mmgr = MemoryManager::GetInstance();
			auto ptr = mmgr.FallbackAllocate(GetID(), parentID, nBytes);

			return ptr;
		}

		void DeallocateBytes(void* ptr, size_t nBytes)
		{
			if (immediateBlock == nullptr && nBytes <= BlockSizeInBytes)
			{
				immediateBlock = ptr;
				return;
			}

			if (!IsValidPointer(ptr))
			{
				// Fallback Deallocation
				auto& mmgr = MemoryManager::GetInstance();
				mmgr.Deallocate(parentID, ptr, nBytes);
				return;
			}

			// Return a block
			WritePointerToNext(ptr, availableBlock);
			availableBlock = ptr;

#if PROFILE_ENABLED
			auto& mmgr = MemoryManager::GetInstance();
			mmgr.ReportDeallocation(id, ptr, nBytes, BlockSizeInBytes);
#endif // PROFILE_ENABLED
		}

		void RegisterAllocator()
		{
			auto& mmgr = MemoryManager::GetInstance();
			auto allocFunc = [](void* allocatorPtr, size_t nBytes) -> void*
			{
				auto allocator = static_cast<InlinePoolAllocator*>(allocatorPtr);
				return static_cast<void*>(allocator->AllocateBytes(nBytes));
			};

			auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t nBytes)
			{
				auto allocator = static_cast<InlinePoolAllocator*>(allocatorPtr);
				allocator->DeallocateBytes(ptr, nBytes);
			};

			const auto capacity = ActualBlockSize * ActualNumBlocks * sizeof(T);
			id = mmgr.RegisterAllocator(this, "InlinePoolAllocator", true, capacity, allocFunc, deallocFunc);

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
			mmgr.DeregisterAllocator(id);
			id = InvalidAllocatorID;
		}

		bool IsValidPointer(void* ptr) const
		{
			constexpr size_t LastBlockIndex = ActualNumBlocks - 1;
			return block[0] <= ptr && ptr <= block[LastBlockIndex];
		}

		static void WritePointerToNext(void* ptr, void* nextPtr)
		{
			void** ptrArray = static_cast<void**>(ptr);
			auto& pointerToNext = reinterpret_cast<void*&>(ptrArray[0]);
			pointerToNext = nextPtr;
		}

		static void* GetPointerToNext(void* ptr)
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

	class InlinePoolAllocatorTest : public TestCollection
	{
	public:
		InlinePoolAllocatorTest() : TestCollection("InlinePoolAllocatorTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
