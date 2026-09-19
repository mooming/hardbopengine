// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>

#include "Container/Array.h"
#include "Memory/NamedPoolAllocator.h"

namespace hbe
{

class MultiPoolAllocator;

/// @brief One result, in flight from the task that produced it to whoever consumes the stream's results.
/// @details Fixed at 128 bytes so that a slot address is the buffer plus a shift, so that 8192 of them fill a
///          megabyte, and so that the layout cannot depend on what a particular result happens to hold. What the
///          bytes mean is nobody's business here - a producer and a consumer agree on that through the result's
///          kind, not through this type.
/// @details The alignment is the engine's maximum fundamental alignment, which makes every slot in an array of
///          them at least 8-byte aligned - the guarantee a result needs in order to keep a pointer or a small
///          header inside the slot without a padding rule of its own.
struct alignas(alignof(std::max_align_t)) TResultSlot
{
	/// @brief Bytes in one slot: 128, fixed by decision rather than by measurement.
	static constexpr std::size_t SizeBytes = 128;

	std::uint8_t bytes[SizeBytes];
};

static_assert(sizeof(TResultSlot) == TResultSlot::SizeBytes, "A result slot must be exactly its declared width.");
static_assert(alignof(TResultSlot) % 8 == 0, "A result slot must stay at least 8-byte aligned.");

/// @brief One owner's run of result slots, appended to in arrival order and reused by rewinding rather than by
///        freeing.
/// @details A producer claims slots in order; nothing frees an individual slot. Once the consumer has finished
///          with the whole run, Rewind hands every slot back at once by putting the count of used slots back to
///          zero, so the same memory serves the next run without a free, a free list, or an allocation anywhere
///          on the path a task runs. That makes the only removal operation this container has a bulk one, which in
///          turn is why an entry may not be erased on its own: an index would mean something different to
///          whoever else is holding it.
/// @details The epoch distinguishes those two uses of an index. Rewind advances it, so a reference to a slot
///          carries which run it was claimed in, and a bump allocator's inability to notice a stale index - a
///          stale index simply reads whichever slot lands there next - stops being an invisible defect.
/// @details Capacity is exact and is only ever changed by an explicit request: Append never grows the buffer, so
///          arriving at a full container is a caller failing to secure room rather than a condition to handle
///          here, and a grow adds exactly the number of slots it was given. A ceiling that a caller states is a
///          number it can keep reasoning with only if nothing rounds it up behind its back.
/// @note The buffer is the owner's: it is allocated from the pool passed to the constructor, so the results a
///       stream produced are accounted to that stream and live in its memory. That pool is not thread-safe, and
///       this container does not make it one - one thread at a time may allocate into it or free from it, which
///       in practice is the owning stream while it runs, and the teardown path once every stream has been joined.
/// @note Appending and rewinding belong to exactly one thread. GetCount and GetEpoch read by another thread are
///       diagnostic: they report the last write that completed, not a snapshot the reader may act on. Capacity is
///       stable to read from anywhere between two grows.
/// @note Growing moves every slot into a new buffer, so a pointer or reference to a slot does not survive a grow
///       and an index plus the epoch does. That is the only reason the two are safe to store, and the reason a
///       handle must not carry an address.
class ResultContainer final
{
public:
	/// @brief Width of one slot in bytes, taken from the slot type so that the two cannot disagree.
	static constexpr std::size_t SlotSizeBytes = TResultSlot::SizeBytes;

	using TSlot = TResultSlot;

private:
	using TSlots = Array<TResultSlot, NamedPoolAllocator<TResultSlot>>;

public:
	/// @brief Build a container holding initialCapacitySlots slots, allocated from ownerAllocator.
	/// @param ownerAllocator The owning stream's pool. Results are allocated and freed only through it, so the
	///        caller is stating whose memory the results live in and which pool has to outlive the container.
	/// @param initialCapacitySlots How many slots the container may hold without growing. Zero takes no memory at
	///        all and accepts nothing either, which is the state of a stream that was never given a capacity.
	/// @note The slots are allocated up front rather than on the first Append, because an allocation on the path a
	///       task runs is the one cost this design refuses to pay. Growing is a separate, deliberate call.
	ResultContainer(MultiPoolAllocator& ownerAllocator, std::size_t initialCapacitySlots) noexcept;

	~ResultContainer() = default;

	ResultContainer(const ResultContainer&) = delete;
	ResultContainer& operator=(const ResultContainer&) = delete;

	/// @brief Claim the next slot, moving the write cursor forward by one.
	/// @return The slot, for the calling thread to fill.
	/// @note Never grows. Room is guaranteed before the work that fills it is taken on, so reaching a full
	///       container is a programming error and is caught by an assert rather than returned as a failure.
	/// @note Do not write beyond SlotSizeBytes, and do not use a slot after the container has been rewound or
	///       grown: a rewind makes the index belong to the next run, and a grow moves the bytes.
	[[nodiscard]] TResultSlot* Append() noexcept;

	/// @brief Hand every slot back at once and advance the epoch.
	/// @details Only the used-slot count changes, so the next Appends overwrite the old slots instead of freeing
	///          them. Advancing the epoch is what makes a reference to the run that just ended recognisable as
	///          stale rather than a silent reading of somebody else's result.
	void Rewind() noexcept
	{
		count = 0;
		++epoch;
	}

	/// @brief Add slotsToAdd slots to the container, keeping the results already written.
	/// @details Grows by exactly that many slots and never by a growth policy's rounding, so a caller that has
	///          worked out a ceiling still has it. It belongs on a pass where growth is expected - a run that is
	///          discovering a shortfall should be refusing work, not reallocating.
	/// @note Moves every slot, including the ones already filled, so anything holding a slot address across this
	///       call is left pointing into a freed buffer. Asserts if the container did not end up at the requested
	///       capacity, because a grow that failed quietly would be a room shortfall discovered by a task.
	void Grow(std::size_t slotsToAdd) noexcept;

	/// @brief The slot at index, for reading a result that was claimed earlier.
	/// @details A consumer walks the slots it is entitled to by index, which is also why a slot is identified by
	///          an index plus the epoch it was claimed in and never by its address.
	/// @note Slots at or beyond GetCount have not been claimed since the last Rewind, so they hold whatever the
	///       previous run left behind rather than a result.
	/// @note Bounds are checked with a fatal assert - the same rule as the array underneath it, which aborts in
	///       every configuration rather than only in a debug build.
	[[nodiscard]] TResultSlot& GetSlot(std::size_t index) noexcept
	{
		return slots[static_cast<TSlots::TIndex>(index)];
	}

	/// @brief Slots claimed since the last Rewind.
	[[nodiscard]] std::size_t GetCount() const noexcept
	{
		return count;
	}

	/// @brief Slots this container can hold without growing. Zero for one that was built with no capacity.
	[[nodiscard]] std::size_t GetCapacity() const noexcept
	{
		return static_cast<std::size_t>(slots.Size());
	}

	/// @brief How many times this container has been rewound, which is which run an index belongs to.
	[[nodiscard]] std::uint64_t GetEpoch() const noexcept
	{
		return epoch;
	}

private:
	TSlots slots;
	std::size_t count = 0;
	std::uint64_t epoch = 0;
};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for the fixed-slot, rewind-to-reuse result container.
class ResultContainerTest final : public TestCollection
{
public:
	ResultContainerTest()
		: TestCollection("ResultContainerTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
