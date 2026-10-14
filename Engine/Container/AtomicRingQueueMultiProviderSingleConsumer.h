// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "Config/BuildConfig.h"
#include "Core/CommonMacros.h"
#include "Core/Debug.h"
#include "Memory/AlignedStorage.h"
#include "Memory/DefaultAllocator.h"


namespace hbe
{
/// API reference: docs/Container/AtomicRingQueueMultiProviderSingleConsumer/index.html
template <typename T, size_t BlockSlots = 1024, class TAllocator = DefaultAllocator<T>>
class AtomicRingQueueMultiProviderSingleConsumer final
{
public:
	enum class EPushResult : unsigned char
	{
		Pushed,
		Full,
		TooManyProducers
	};

	enum class EPopResult : unsigned char
	{
		Element,
		Empty,
		Blocked
	};

private:
	struct LinkNode;

	using StorageAllocator = typename TAllocator::template rebind<std::uint8_t>::other;
	using DeferredAllocator = typename TAllocator::template rebind<LinkNode*>::other;
	using DeferredList = std::vector<LinkNode*, DeferredAllocator>;

	static_assert(BlockSlots > 1 && (BlockSlots & (BlockSlots - 1)) == 0,
				  "BlockSlots must be a power of two, so a slot index is one mask and a block ordinal one shift.");

	static_assert(sizeof(T) % alignof(T) == 0,
				  "sizeof(T) must be a multiple of alignof(T), so a stride of sizeof(T) keeps every slot aligned.");

	static constexpr size_t AlignSize = std::hardware_destructive_interference_size;
	static constexpr size_t AlignOfT = alignof(T);

	struct alignas(AlignSize) LinkNode final // hb-standards:ignore
	{
		static constexpr size_t InvalidTag = 0;

		alignas(AlignSize) std::atomic<size_t> claim;
		AlignedStorage<LinkNode> allocation;
		std::atomic<LinkNode*> next;
		alignas(AlignSize) std::atomic<size_t> tags[BlockSlots];
		alignas(AlignOfT) std::byte storage[BlockSlots * sizeof(T)];

		LinkNode()
			: claim(0)
			, tags{}
			, storage{}
		{
		}
	};

	struct alignas(AlignSize) ProducerSlot final // hb-standards:ignore
	{
		std::atomic<LinkNode*> block = nullptr;
		std::atomic<bool> claimed = false;
	};

	static_assert((alignof(ProducerSlot) ^ (alignof(ProducerSlot) - 1)) > alignof(ProducerSlot) - 1,
				  "AtomicRingQueueMPSC: slot spacing must be a power of two to round an address up with one mask.");

	static_assert(std::is_trivially_destructible_v<ProducerSlot>,
				  "AtomicRingQueueMPSC: the slot array is placement-new and freed as raw bytes; no destructor runs.");

	static constexpr size_t InvalidProducerToken = static_cast<size_t>(-1);
	static constexpr size_t SlotMask = BlockSlots - 1;
	static inline thread_local size_t producerToken = InvalidProducerToken;

private:
	alignas(AlignSize) std::mutex managementLock;
	size_t blockCount;
	size_t maxBlocks;

	alignas(AlignSize) std::atomic<LinkNode*> chainRoot;
	std::atomic<LinkNode*> tail;

	alignas(AlignSize) LinkNode* head;
	size_t headCursor;

	alignas(AlignSize) TAllocator allocator;
	AlignedStorage<ProducerSlot> slotAllocation;
	ProducerSlot* producerSlots;
	size_t producerSlotCount;
	DeferredList deferredBlocks;

	alignas(AlignSize) std::atomic<size_t> growCount;
	std::atomic<size_t> retireCount;
	std::atomic<size_t> blockedCount;

public:
	AtomicRingQueueMultiProviderSingleConsumer(const AtomicRingQueueMultiProviderSingleConsumer&) = delete;
	AtomicRingQueueMultiProviderSingleConsumer& operator=(const AtomicRingQueueMultiProviderSingleConsumer&) = delete;

	explicit AtomicRingQueueMultiProviderSingleConsumer(size_t inMaxBlocks = 64, size_t inMaxProducers = 64) noexcept
		: blockCount(0)
		, maxBlocks(inMaxBlocks)
		, chainRoot(nullptr)
		, tail(nullptr)
		, head(nullptr)
		, headCursor(0)
		, slotAllocation()
		, producerSlots(nullptr)
		, producerSlotCount(0)
		, deferredBlocks(DeferredAllocator{allocator})
		, growCount(0)
		, retireCount(0)
		, blockedCount(0)
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

	~AtomicRingQueueMultiProviderSingleConsumer()
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

		LinkNode* block = nullptr;
		for (;;)
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
			if (claimIndex >= BlockSlots)
			{
				LinkNode* next = block->next.load(std::memory_order_acquire);
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

			T* slot = SlotAt(*block, claimIndex);
			new (slot) T(inItem);
			block->tags[claimIndex & SlotMask].store(claimIndex + 1, std::memory_order_release);

			producerSlots[token].block.store(nullptr, std::memory_order_release);

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

		LinkNode* block = head;
		while (true)
		{
			const size_t claimCount = block->claim.load(std::memory_order_acquire);
			if (headCursor < claimCount)
			{
				const size_t slotIndex = headCursor & SlotMask;
				if (block->tags[slotIndex].load(std::memory_order_acquire) == headCursor + 1)
				{
					T* slot = SlotAt(*block, headCursor);
					outItem = std::move(*slot);
					slot->~T();
					block->tags[slotIndex].store(LinkNode::InvalidTag, std::memory_order_relaxed);
					++headCursor;

					return EPopResult::Element;
				}

				blockedCount.fetch_add(1, std::memory_order_relaxed);

				return EPopResult::Blocked;
			}

			LinkNode* next = block->next.load(std::memory_order_acquire);
			returnValueIf(EPopResult::Empty, next == nullptr);

			head = next;
			headCursor = 0;
			RetireBlock(block, next);
			block = next;
		}
	}

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		LinkNode* block = head;
		if (block == nullptr)
		{
			block = chainRoot.load(std::memory_order_acquire);
			returnValueIf(true, block == nullptr);
		}

		return headCursor >= block->claim.load(std::memory_order_acquire) &&
			   block->next.load(std::memory_order_acquire) == nullptr;
	}

	[[nodiscard]] size_t GetGrowCount() const noexcept
	{
		return growCount.load(std::memory_order_relaxed);
	}

	[[nodiscard]] size_t GetRetireCount() const noexcept
	{
		return retireCount.load(std::memory_order_relaxed);
	}

	[[nodiscard]] size_t GetBlockedCount() const noexcept
	{
		return blockedCount.load(std::memory_order_relaxed);
	}

private:
	[[nodiscard]] static T* SlotAt(LinkNode& inBlock, size_t inIndex) noexcept
	{
		return reinterpret_cast<T*>(inBlock.storage + (inIndex & SlotMask) * sizeof(T));
	}

	[[nodiscard]] LinkNode* AllocateBlock() noexcept
	{
		StorageAllocator storageAllocator{allocator};
		auto allocation = AlignedStorage<LinkNode>::Allocate(storageAllocator, 1);
		returnValueIf(nullptr, unlikely(allocation.raw == nullptr));

		auto* block = new (allocation.aligned) LinkNode();

		block->allocation = allocation;
		block->claim.store(0, std::memory_order_relaxed);
		block->next.store(nullptr, std::memory_order_relaxed);
		for (size_t index = 0; index < BlockSlots; ++index)
		{
			block->tags[index].store(LinkNode::InvalidTag, std::memory_order_relaxed);
		}

		return block;
	}

	[[nodiscard]] LinkNode* AcquireBlock(LinkNode* inFullBlock) noexcept
	{
		std::lock_guard lockGuard(managementLock);

		RetryDeferredBlocks();

		if (inFullBlock != nullptr)
		{
			LinkNode* installed = inFullBlock->next.load(std::memory_order_acquire);
			returnValueIf(installed, installed != nullptr);
		}

		returnValueIf(nullptr, blockCount >= maxBlocks);

		LinkNode* block = AllocateBlock();
		returnValueIf(nullptr, unlikely(block == nullptr));

		++blockCount;
		growCount.fetch_add(1, std::memory_order_relaxed);

		if (inFullBlock == nullptr)
		{
			LinkNode* expected = nullptr;
			if (!chainRoot.compare_exchange_strong(expected, block, std::memory_order_release,
												   std::memory_order_acquire))
			{
				FreeBlock(block);
				--blockCount;
				--growCount;
			}
			else
			{
				tail.store(block, std::memory_order_release);
			}

			return chainRoot.load(std::memory_order_acquire);
		}

		LinkNode* expected = nullptr;
		if (!inFullBlock->next.compare_exchange_strong(expected, block, std::memory_order_release,
													   std::memory_order_acquire))
		{
			FreeBlock(block);
			--blockCount;
			--growCount;
			block = expected;
		}

		tail.compare_exchange_strong(inFullBlock, block, std::memory_order_acq_rel, std::memory_order_relaxed);

		return block;
	}

	[[nodiscard]] size_t AcquireProducerToken() noexcept
	{
		size_t token = producerToken;
		returnValueIf(token, token != InvalidProducerToken);

		std::lock_guard lockGuard(managementLock);
		for (size_t index = 0; index < producerSlotCount; ++index)
		{
			bool expected = false;
			if (producerSlots[index].claimed.compare_exchange_strong(expected, true, std::memory_order_relaxed))
			{
				token = index;
				break;
			}
		}

		Assert(token != InvalidProducerToken,
			   "AtomicRingQueueMPSC: every producer slot is claimed, so one more producer thread cannot be tracked; "
			   "raise inMaxProducers at construction.");

		producerToken = token;

		return token;
	}

	void FreeBlock(LinkNode* inBlock) noexcept
	{
		StorageAllocator storageAllocator{allocator};
		inBlock->allocation.Free(storageAllocator);
	}

	[[nodiscard]] bool IsBlockShadowed(LinkNode* inBlock) const noexcept
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
		for (LinkNode* block : deferredBlocks)
		{
			continueIf(IsBlockShadowed(block));

			FreeBlock(block);
			--blockCount;
			++freed;
		}

		size_t kept = 0;
		for (LinkNode* block : deferredBlocks)
		{
			continueIf(!IsBlockShadowed(block));
			deferredBlocks[kept++] = block;
		}

		deferredBlocks.resize(kept);
		retireCount.fetch_add(freed, std::memory_order_relaxed);
	}

	void RetireBlock(LinkNode* inBlock, LinkNode* inSuccessor) noexcept
	{
		std::lock_guard lockGuard(managementLock);

		LinkNode* expected = inBlock;
		tail.compare_exchange_strong(expected, inSuccessor, std::memory_order_seq_cst, std::memory_order_relaxed);

		if (IsBlockShadowed(inBlock))
		{
			deferredBlocks.push_back(inBlock);

			return;
		}

		RetryDeferredBlocks();

		FreeBlock(inBlock);
		--blockCount;
		retireCount.fetch_add(1, std::memory_order_relaxed);
	}

	void DestroyAll() noexcept
	{
		LinkNode* block = head != nullptr ? head : chainRoot.load(std::memory_order_relaxed);
		size_t index = head != nullptr ? headCursor : 0;

		while (block != nullptr)
		{
			const size_t claimCount = block->claim.load(std::memory_order_relaxed);
			for (; index < claimCount; ++index)
			{
				if (block->tags[index & SlotMask].load(std::memory_order_relaxed) == index + 1)
				{
					T* slot = SlotAt(*block, index);
					slot->~T();
					block->tags[index & SlotMask].store(LinkNode::InvalidTag, std::memory_order_relaxed);
				}
			}

			LinkNode* next = block->next.load(std::memory_order_relaxed);
			FreeBlock(block);
			block = next;
			index = 0;
		}

		for (LinkNode* deferred : deferredBlocks)
		{
			FreeBlock(deferred);
		}

		deferredBlocks.clear();

		tail.store(nullptr, std::memory_order_relaxed);
		chainRoot.store(nullptr, std::memory_order_relaxed);
		head = nullptr;
		headCursor = 0;
		blockCount = 0;
	}
};

template <typename T, size_t BlockSlots = 1024>
using AtomicRingQueueMPSC = AtomicRingQueueMultiProviderSingleConsumer<T, BlockSlots>;
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
class AtomicRingQueueMultiProviderSingleConsumerTest : public TestCollection
{
public:
	AtomicRingQueueMultiProviderSingleConsumerTest()
		: TestCollection("AtomicRingQueueMultiProviderSingleConsumerTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe

#endif // __UNIT_TEST__
