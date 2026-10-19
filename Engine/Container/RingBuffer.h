// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "Config/BuildConfig.h"
#include "Container/AtomicFixedRingMPSC.h"
#include "Container/EPopResult.h"
#include "Container/EPushResult.h"
#include "Core/CommonMacros.h"
#include "Core/Debug.h"
#include "Memory/AlignedStorage.h"
#include "Memory/DefaultAllocator.h"


namespace hbe
{
template <typename T, typename U>
concept CFixedRing = std::same_as<T, typename U::ValueType> && std::default_initializable<U> &&
					 !std::copy_constructible<U> && requires(U& ring, const T& item, T&& movedItem, T& outItem) {
						 { U::GetCapacity() } -> std::same_as<std::size_t>;
						 { ring.Push(item) } -> std::same_as<EPushResult>;
						 { ring.Push(std::move(movedItem)) } -> std::same_as<EPushResult>;
						 { ring.Pop(outItem) } -> std::same_as<EPopResult>;
						 { ring.IsEmpty() } -> std::same_as<bool>;
						 { ring.GetSlotVersion() } -> std::same_as<std::size_t>;
					 } && (U::GetCapacity() > 1) && ((U::GetCapacity() & (U::GetCapacity() - 1)) == 0);

template <typename T, typename U, class TAllocator = DefaultAllocator<T>>
	requires CFixedRing<T, U>
/// API reference: docs/Container/RingBuffer/index.html
class RingBuffer final
{
private:
	struct Block;

	using StorageAllocator = typename TAllocator::template rebind<std::uint8_t>::other;
	using DeferredAllocator = typename TAllocator::template rebind<Block*>::other;
	using DeferredList = std::vector<Block*, DeferredAllocator>;

	static constexpr size_t LineSpacing = std::hardware_destructive_interference_size;
	static constexpr size_t BlockCapacity = U::GetCapacity();

	struct alignas(LineSpacing) Block final // hb-standards:ignore
	{
		alignas(LineSpacing) std::atomic<size_t> claim;
		AlignedStorage<Block> allocation;
		std::atomic<Block*> next;
		alignas(LineSpacing) U ring;

		Block()
			: claim(0)
			, allocation()
			, next(nullptr)
			, ring()
		{
		}
	};

	struct alignas(LineSpacing) ProducerSlot final // hb-standards:ignore
	{
		std::atomic<Block*> block = nullptr;
		std::atomic<bool> claimed = false;
	};

	static_assert((alignof(ProducerSlot) ^ (alignof(ProducerSlot) - 1)) > alignof(ProducerSlot) - 1,
				  "RingBuffer: slot spacing must be a power of two to round an address up with one mask.");

	static_assert(std::is_trivially_destructible_v<ProducerSlot>,
				  "RingBuffer: the slot array is placement-new and freed as raw bytes; no destructor runs.");

	static constexpr size_t InvalidProducerToken = static_cast<size_t>(-1);
	static inline std::atomic<size_t> nextInstanceId{1};
	static inline thread_local const void* producerCacheInstance = nullptr;
	static inline thread_local size_t producerCacheId = 0;
	static inline thread_local size_t producerToken = InvalidProducerToken;

private:
	alignas(LineSpacing) std::mutex managementLock;
	std::atomic<size_t> blockCount;
	size_t maxBlocks;

	alignas(LineSpacing) std::atomic<Block*> chainRoot;
	std::atomic<Block*> tail;

	alignas(LineSpacing) Block* head;

	alignas(LineSpacing) TAllocator allocator;
	AlignedStorage<ProducerSlot> slotAllocation;
	ProducerSlot* producerSlots;
	size_t producerSlotCount;
	const size_t instanceId;
	DeferredList deferredBlocks;

	alignas(LineSpacing) std::atomic<size_t> growCount;
	std::atomic<size_t> retireCount;
	std::atomic<size_t> blockedCount;
	std::atomic<size_t> fuseRefusalCount;
	std::atomic<size_t> deferredBlockCount;

public:
	RingBuffer(const RingBuffer&) = delete;
	RingBuffer& operator=(const RingBuffer&) = delete;

	explicit RingBuffer(size_t inMaxBlocks = 64, size_t inMaxProducers = 64) noexcept
		: blockCount(0)
		, maxBlocks(inMaxBlocks)
		, chainRoot(nullptr)
		, tail(nullptr)
		, head(nullptr)
		, slotAllocation()
		, producerSlots(nullptr)
		, producerSlotCount(0)
		, instanceId(nextInstanceId.fetch_add(1, std::memory_order_relaxed))
		, deferredBlocks(DeferredAllocator{allocator})
		, growCount(0)
		, retireCount(0)
		, blockedCount(0)
		, fuseRefusalCount(0)
		, deferredBlockCount(0)
	{
		deferredBlocks.reserve(inMaxBlocks);
		returnIf(inMaxProducers == 0);

		StorageAllocator storageAllocator{allocator};
		slotAllocation = AlignedStorage<ProducerSlot>::Allocate(storageAllocator, inMaxProducers);
		returnIf(unlikely(slotAllocation.raw == nullptr));

		producerSlots = new (slotAllocation.aligned) ProducerSlot[inMaxProducers];
		producerSlotCount = inMaxProducers;

		for (size_t index = 0; index < producerSlotCount; ++index)
		{
			producerSlots[index].block.store(nullptr, std::memory_order_relaxed);
			producerSlots[index].claimed.store(false, std::memory_order_relaxed);
		}
	}

	~RingBuffer()
	{
		DestroyAll();

		returnIf(producerSlots == nullptr);

		StorageAllocator storageAllocator{allocator};
		slotAllocation.Free(storageAllocator);
	}

	[[nodiscard]] EPushResult Push(const T& inItem) noexcept
	{
		const size_t token = AcquireProducerToken();
		returnValueIf(EPushResult::TooManyProducers, unlikely(token == InvalidProducerToken));

		Block* block = nullptr;

		while (true)
		{
			block = tail.load(std::memory_order_seq_cst);
			if (block == nullptr)
			{
				breakIf(AcquireBlock(nullptr) == nullptr);

				continue;
			}

			producerSlots[token].block.store(block, std::memory_order_seq_cst);
			continueIf(tail.load(std::memory_order_seq_cst) != block);

			size_t claimIndex = block->claim.load(std::memory_order_relaxed);
			if (claimIndex >= BlockCapacity)
			{
				Block* next = block->next.load(std::memory_order_acquire);
				if (next == nullptr)
				{
					breakIf(AcquireBlock(block) == nullptr);

					continue;
				}

				tail.compare_exchange_strong(block, next, std::memory_order_acq_rel, std::memory_order_relaxed);
				continue;
			}

			if (!block->claim.compare_exchange_weak(claimIndex, claimIndex + 1, std::memory_order_acq_rel,
													std::memory_order_relaxed))
			{
				continue;
			}

			const EPushResult ringResult = block->ring.Push(inItem);

			producerSlots[token].block.store(nullptr, std::memory_order_release);

			Assert(unlikely(ringResult != EPushResult::Full),
				   "RingBuffer: the block answered Full although its claim had room, so one lap is not enforced.");
			returnValueIf(EPushResult::Full, ringResult == EPushResult::Full);

			return EPushResult::Pushed;
		}

		producerSlots[token].block.store(nullptr, std::memory_order_release);

		return EPushResult::Full;
	}

	[[nodiscard]] EPopResult Pop(T& outItem) noexcept
	{
		if (head == nullptr)
		{
			head = chainRoot.load(std::memory_order_acquire);
			returnValueIf(EPopResult::Empty, head == nullptr);
		}

		Block* block = head;
		while (true)
		{
			returnValueIf(EPopResult::Element, block->ring.Pop(outItem) == EPopResult::Element);

			const bool reservedAhead = block->claim.load(std::memory_order_acquire) > block->ring.GetSlotVersion();
			if (reservedAhead || !block->ring.IsEmpty())
			{
				blockedCount.fetch_add(1, std::memory_order_relaxed);

				return EPopResult::Blocked;
			}

			Block* next = block->next.load(std::memory_order_acquire);
			returnValueIf(EPopResult::Empty, next == nullptr);

			head = next;
			RetireBlock(block, next);
			block = next;
		}
	}

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		Block* block = head;
		if (block == nullptr)
		{
			block = chainRoot.load(std::memory_order_acquire);
			returnValueIf(true, block == nullptr);
		}

		return block->ring.IsEmpty() && block->next.load(std::memory_order_acquire) == nullptr;
	}

	[[nodiscard]] size_t GetGrowCount() const noexcept
	{
		return growCount.load(std::memory_order_relaxed);
	}

	[[nodiscard]] size_t GetRetireCount() const noexcept
	{
		return retireCount.load(std::memory_order_relaxed);
	}

	[[nodiscard]] size_t GetFuseRefusalCount() const noexcept
	{
		return fuseRefusalCount.load(std::memory_order_relaxed);
	}

	[[nodiscard]] size_t GetDeferredBlockCount() const noexcept
	{
		return deferredBlockCount.load(std::memory_order_relaxed);
	}

	[[nodiscard]] size_t GetBlockedCount() const noexcept
	{
		return blockedCount.load(std::memory_order_relaxed);
	}

private:
	[[nodiscard]] Block* AllocateBlock() noexcept
	{
		StorageAllocator storageAllocator{allocator};
		auto allocation = AlignedStorage<Block>::Allocate(storageAllocator, 1);
		returnValueIf(nullptr, unlikely(allocation.raw == nullptr));

		auto* block = new (allocation.aligned) Block();

		block->allocation = allocation;
		block->claim.store(0, std::memory_order_relaxed);
		block->next.store(nullptr, std::memory_order_relaxed);

		return block;
	}

	[[nodiscard]] Block* AcquireBlock(Block* inFullBlock) noexcept
	{
		std::lock_guard lockGuard(managementLock);

		RetryDeferredBlocks();

		if (inFullBlock != nullptr)
		{
			Block* installed = inFullBlock->next.load(std::memory_order_acquire);
			returnValueIf(installed, installed != nullptr);
		}

		if (blockCount.load(std::memory_order_relaxed) >= maxBlocks)
		{
			fuseRefusalCount.fetch_add(1, std::memory_order_relaxed);

			return nullptr;
		}

		Block* block = AllocateBlock();
		returnValueIf(nullptr, unlikely(block == nullptr));

		blockCount.fetch_add(1, std::memory_order_relaxed);
		growCount.fetch_add(1, std::memory_order_relaxed);

		if (inFullBlock == nullptr)
		{
			Block* expected = nullptr;
			if (!chainRoot.compare_exchange_strong(expected, block, std::memory_order_release,
												   std::memory_order_acquire))
			{
				FreeBlock(block);
				blockCount.fetch_sub(1, std::memory_order_relaxed);
				--growCount;
			}
			else
			{
				tail.store(block, std::memory_order_release);
			}

			return chainRoot.load(std::memory_order_acquire);
		}

		Block* expected = nullptr;
		if (!inFullBlock->next.compare_exchange_strong(expected, block, std::memory_order_release,
													   std::memory_order_acquire))
		{
			FreeBlock(block);
			blockCount.fetch_sub(1, std::memory_order_relaxed);
			--growCount;
			block = expected;
		}

		tail.compare_exchange_strong(inFullBlock, block, std::memory_order_acq_rel, std::memory_order_relaxed);

		return block;
	}

	[[nodiscard]] size_t AcquireProducerToken() noexcept
	{
		if (producerToken != InvalidProducerToken && producerCacheInstance == this && producerCacheId == instanceId &&
			producerSlots[producerToken].claimed.load(std::memory_order_acquire))
		{
			return producerToken;
		}

		std::lock_guard lockGuard(managementLock);

		size_t token = InvalidProducerToken;
		for (size_t index = 0; index < producerSlotCount; ++index)
		{
			bool expected = false;
			if (producerSlots[index].claimed.compare_exchange_strong(expected, true, std::memory_order_acquire,
																	 std::memory_order_relaxed))
			{
				token = index;
				break;
			}
		}

		producerCacheInstance = this;
		producerCacheId = instanceId;
		producerToken = token;

		return token;
	}

	void FreeBlock(Block* inBlock) noexcept
	{
		StorageAllocator storageAllocator{allocator};
		const AlignedStorage<Block> allocation = inBlock->allocation;

		inBlock->~Block();
		allocation.Free(storageAllocator);
	}

	[[nodiscard]] bool IsBlockShadowed(Block* inBlock) const noexcept
	{
		for (size_t index = 0; index < producerSlotCount; ++index)
		{
			returnValueIf(true, producerSlots[index].block.load(std::memory_order_seq_cst) == inBlock);
		}

		return false;
	}

	void RetryDeferredBlocks() noexcept
	{
		returnIf(deferredBlocks.empty());

		size_t freed = 0;
		for (Block* block : deferredBlocks)
		{
			continueIf(IsBlockShadowed(block));

			FreeBlock(block);
			blockCount.fetch_sub(1, std::memory_order_relaxed);
			++freed;
		}

		size_t kept = 0;
		for (Block* block : deferredBlocks)
		{
			continueIf(!IsBlockShadowed(block));
			deferredBlocks[kept++] = block;
		}

		deferredBlocks.resize(kept);
		deferredBlockCount.store(deferredBlocks.size(), std::memory_order_relaxed);
		retireCount.fetch_add(freed, std::memory_order_relaxed);
	}

	void RetireBlock(Block* inBlock, Block* inSuccessor) noexcept
	{
		std::lock_guard lockGuard(managementLock);

		Block* expected = inBlock;
		tail.compare_exchange_strong(expected, inSuccessor, std::memory_order_seq_cst, std::memory_order_relaxed);

		if (IsBlockShadowed(inBlock))
		{
			deferredBlocks.push_back(inBlock);
			deferredBlockCount.store(deferredBlocks.size(), std::memory_order_relaxed);

			return;
		}

		RetryDeferredBlocks();

		FreeBlock(inBlock);
		blockCount.fetch_sub(1, std::memory_order_relaxed);
		retireCount.fetch_add(1, std::memory_order_relaxed);
	}

	void DestroyAll() noexcept
	{
		Block* block = head != nullptr ? head : chainRoot.load(std::memory_order_relaxed);

		while (block != nullptr)
		{
			Block* next = block->next.load(std::memory_order_relaxed);
			FreeBlock(block);
			block = next;
		}

		for (Block* deferred : deferredBlocks)
		{
			FreeBlock(deferred);
		}

		deferredBlocks.clear();
		deferredBlockCount.store(0, std::memory_order_relaxed);

		tail.store(nullptr, std::memory_order_relaxed);
		chainRoot.store(nullptr, std::memory_order_relaxed);
		head = nullptr;
		blockCount.store(0, std::memory_order_relaxed);
	}
};

template <typename T, size_t BlockCapacity = 1024>
using RingBufferMPSC = RingBuffer<T, AtomicFixedRingMPSC<T, BlockCapacity>>;
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Container/AtomicFixedRingMPMC.h"
#include "Container/FixedRingMPMC.h"
#include "Container/FixedRingMPSC.h"
#include "Test/TestCollection.h"

namespace hbe
{
static_assert(CFixedRing<int, AtomicFixedRingMPSC<int, 4>>,
			  "RingBuffer: the lock-free single-consumer ring must be usable as a block.");
static_assert(CFixedRing<int, AtomicFixedRingMPMC<int, 4>>,
			  "RingBuffer: the lock-free multi-consumer ring must be usable as a block.");
static_assert(CFixedRing<int, FixedRingMPSC<int, 4>>,
			  "RingBuffer: the mutex single-consumer ring must be usable as a block.");
static_assert(CFixedRing<int, FixedRingMPMC<int, 4>>,
			  "RingBuffer: the mutex multi-consumer ring must be usable as a block.");

class RingBufferTest : public TestCollection
{
public:
	RingBufferTest()
		: TestCollection("RingBufferTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe

#endif // __UNIT_TEST__
