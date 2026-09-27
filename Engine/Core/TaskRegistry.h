// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>

#include "Core/Runnable.h"
#include "Memory/AllocatorID.h"
#include "String/StaticString.h"
#include "Task.h"
#include "TaskID.h"

namespace hbe
{
/// @brief Tracks every task the engine knows about, and issues the identity that replaces a task's address.
/// @details A task used to be identified by where it sat, so anything holding a reference to one was trusting that
///          nobody had destroyed it. Here a task lives in a record, and the identity handed out is the record's
///          index plus a generation that changes whenever the record is issued again. A reference to a task that
///          has been released is therefore recognised as stale and dropped, instead of being followed into
///          whatever task now occupies the record. See TaskID.
///
/// @details __Storage.__ Records are held in banks of a fixed number of each, one bank per grow. Records are never
///          moved, which is what makes three things at once possible: a task holds an atomic counter and so has no
///          move constructor to give a growing array; an index stays meaningful across growth; and a task pointer
///          returned by Find cannot be invalidated underneath its caller. Banks are allocated one at a time from a
///          named allocator, exact-sized, because a pool bank rounds a table of this size up by more than the table
///          itself - see the registry section of docs/TaskSystemRedesign.md.
///
/// @details __Capacity.__ Three knobs, the same shape as every other growable in the engine: an initial capacity in
///          records, a grow-by, and a ceiling where zero means no ceiling. Growing is always an explicit call.
///          Create never grows the table, so the thread that runs out of records is not the thread that pays for
///          more memory - it gets a null ID and a log line naming what could not be tracked.
///
/// @note __Threads.__ Creating and releasing are serialised under one lock, because tasks are created from whatever
///       thread the caller is on - the logger does exactly that. Find takes no lock: bank pointers are published
///       with release semantics before the bank count grows, and a record's generation and in-use flag are atomic,
///       so a lookup reads a consistent answer without one thread waiting for another.
/// @note __Growing__ is only legal while no lookup is in flight, which in practice means between frames on whoever
///       owns the registry. Nothing grows it from a task: an allocation that lands on the path a task runs makes
///       the task, and not the caller, pay for a table it had no part in sizing.
class TaskRegistry final
{
public:
	/// @brief Records the registry starts with: 4096, which is one bank of 1 MiB.
	/// @details Chosen against a measured demand of one tracked task in engine code - the logger's - so this is
	///          sized for what a game will dispatch rather than for what this tree does today. A record is
	///          RecordSizeBytes bytes - 256 since R28 - so a fresh registry costs 1 MiB. That is the price of
	///          tracking a task at all: identity, generation, a result packet, a successor, and a line of its own.
	static constexpr std::size_t DefaultInitialCapacityRecords = 4096;

	/// @brief Records each grow adds: one bank, the same figure as the initial capacity.
	/// @details Growth is exact rather than a doubling policy, so the cost of the next bank is knowable where the
	///          registry is configured instead of being a surprise at whatever size the table happened to reach.
	static constexpr std::size_t DefaultGrowByRecords = DefaultInitialCapacityRecords;

	/// @brief Most records the table may hold: 0, which means no ceiling.
	/// @details The same convention as a stream's result ceiling, so one rule covers both growables: the inert
	///          value is the default, and a cap is something a caller opts into knowingly.
	static constexpr std::size_t DefaultMaxCapacityRecords = 0;

	/// @brief Banks a table may hold at most, which bounds how far the grow-by can be taken.
	/// @details The bank table itself is a fixed array of pointers so that growing publishes a new bank without
	///          reallocating anything a lookup might be reading. At the default grow-by this allows millions of
	///          records, far beyond what the ceiling is for, and growing past it is refused with a log line naming
	///          the limit rather than being silently wrong.
	static constexpr std::size_t MaxBanks = 1024;

	using TIndex = Task::TIndex;

private:
	struct Record final
	{
		Task task;

		/// @brief The task to dispatch when this one's join closes, or a null ID for "nobody is waiting".
		/// @details Routing lives here rather than in Task, which is R9's rule: the task system knows a job, an
		///          optional successor, and a join counter, and nothing about pipelines. It is registry state on
		///          purpose - the only path allowed to act on it is the one that holds the identity rules.
		TaskID successor;

		std::atomic<TaskID::TGeneration> generation;
		std::atomic<bool> inUse;
		std::size_t nextFreeRecord;

		/// @brief Space held back so every record starts on a cache line. See RecordSizeBytes and R28.
		std::byte reservedToCacheLine[24];
	};

	using TBank = Record*;

public:
	/// @brief Bytes one record occupies, which is the multiplier behind every capacity figure here.
	/// @details Exposed because the cost of a record is the only thing that turns "4096 records" into a memory
	///          figure a caller can argue about, and because it moves when the task itself grows - a registry sized
	///          in records is sized in units nobody can price without reading this.
	static constexpr std::size_t RecordSizeBytes = sizeof(Record);

	/// @brief The record's priced size, asserted rather than quoted, because the table's memory is this figure
	///        times the capacity and a field that quietly changes it changes every engine's memory budget.
	/// @details The successor costs a full TaskID - measured 16 bytes - which takes the record past three cache
	///        lines, so it is padded to four. R28 priced and decided that: the alternatives were a record of 208
	///        bytes, a multiple of 8 that stops records starting on line boundaries so two streams writing results
	///        into neighbouring records invalidate each other through the shared line; aliasing the free-list link,
	///        which is unused while a record is live but caps a successor at a compressed identity and leaves an
	///        overlap a reader has to hold in their head; and a side table, which buys the alignment back with a
	///        lifetime rule and a second read on the delivery path.
	/// @details Cost: 64 bytes per record, so the default 4096-record table is 1 MiB rather than 768 KiB.
	static_assert(RecordSizeBytes == 256, "R28 prices a record at 256 bytes; re-measure and re-decide");
	static_assert(RecordSizeBytes % 64 == 0, "Records must start on cache-line boundaries");

private:
	std::mutex registryLock;
	StaticString name;
	TBank banks[MaxBanks];
	std::atomic<std::size_t> bankCount;
	std::size_t recordsPerBank;
	std::size_t maxCapacityRecords;
	std::size_t freeRecordHead;
	std::atomic<std::size_t> usedRecords;
	TAllocatorID bankAllocatorID;

public:
	/// @brief Build an empty registry: no banks, no records, no memory taken.
	/// @details Call Initialize to give it records. It is a separate call rather than a constructor argument
	///          because a registry lives inside the task system, which is itself default-constructed and sized
	///          later, and because the allocator the banks come from is only known once the engine has chosen it.
	TaskRegistry() noexcept;
	~TaskRegistry() noexcept;

	/// @brief Take initialCapacityRecords records now, from the allocator in scope at this moment.
	/// @details initialCapacityRecords is rounded down to a whole number of banks of growByRecords, and GetCapacity
	///          reports what the table really holds, so a caller that asked for a part bank sees the shortfall here
	///          rather than in a lookup that unexpectedly fails.
	/// @note Call this once, before any task is created. Calling it again while records are in use leaks the banks
	///       it already had, which is why the task system calls it from its own Initialize and nowhere else.
	void Initialize(StaticString registryName, std::size_t initialCapacityRecords, std::size_t growByRecords) noexcept;

	TaskRegistry(const TaskRegistry&) = delete;
	TaskRegistry& operator=(const TaskRegistry&) = delete;

	/// @brief Record which task to dispatch when this one's join closes.
	/// @details This is the whole of what the task system knows about chains (R9): a job, an optional successor,
	///          and a join counter. Whether the successor runs on the completing task's own stream or somewhere
	///          else is decided by the destination byte the completing task wrote into its own result packet, so
	///          routing stays data in the registry and never becomes a pipeline the engine has to understand.
	/// @note A successor is a promise about the future, not a lease: it is dispatched when the join closes, and if
	///       it has been released by then the dispatch is refused and logged rather than run.
	/// @note Naming a task that is not tracked does nothing and is reported, same as Release.
	void SetSuccessor(TaskID task, TaskID successor) noexcept;

	/// @brief The successor recorded for a task, or a null ID when nobody is waiting on its join.
	[[nodiscard]] TaskID GetSuccessor(TaskID task) noexcept;

	/// @brief Track a task and return the identity that names it.
	/// @return The new identity, or a null ID if the table has no free record. A refused task is not tracked, is
	///         not run, and the refusal is logged with the task name and the capacity, because a task that silently
	///         never runs is indistinguishable from one still waiting.
	/// @note This never grows the table. Call Grow where memory may be taken - between frames, on the owner - and
	///       treat a refusal here as a sizing problem to fix there.
	[[nodiscard]] TaskID Create(StaticString taskName, TRunnable func, void* userData) noexcept;

	/// @brief The tracked task an ID names, or nullptr if it names nothing.
	/// @details Nullptr means the record was never issued, the task has been released, or the record has since been
	///          issued to another task - that last case being precisely what the generation exists to catch. A
	///          caller must handle null; there is no variant of this call that returns a task it cannot verify.
	/// @note The returned pointer stays valid memory for the life of the registry. It is not a lease: if the task
	///       is released on another thread while it is being read, the fields read are whatever they held, not a
	///       guarantee about what they say.
	[[nodiscard]] Task* Find(TaskID id) noexcept;

	/// @brief Stop tracking a task, freeing its record for reuse.
	/// @note Releasing an ID that is not live - null, never issued, or already released - is reported and changes
	///       nothing. A double release that went unnoticed would hand one record to two tasks, so it is logged
	///       rather than tolerated.
	void Release(TaskID id) noexcept;

	/// @brief Add one bank of records to the table.
	/// @return False if a configured ceiling or the bank table's own limit refuses it, in which case the capacity is
	///         unchanged and the refusal is logged.
	/// @note Legal only while no lookup is in flight. See the thread note on this class.
	[[nodiscard]] bool Grow() noexcept;

	/// @brief Records the table can hold without growing.
	[[nodiscard]] std::size_t GetCapacity() const noexcept;

	/// @brief Records currently tracking a task.
	/// @note A count read while other threads are creating or releasing is the last one that completed, not a
	///       snapshot a decision may be built on. It is there to be looked at, not to be raced against.
	[[nodiscard]] std::size_t GetCount() const noexcept
	{
		return usedRecords.load(std::memory_order::relaxed);
	}

	/// @brief Records each Grow adds.
	[[nodiscard]] std::size_t GetGrowBy() const noexcept
	{
		return recordsPerBank;
	}

	/// @brief Most records the table may hold. Zero means no ceiling.
	[[nodiscard]] std::size_t GetMaxCapacity() const noexcept
	{
		return maxCapacityRecords;
	}

	/// @brief Cap how large the table may be grown. Zero lifts the cap.
	/// @note This bounds growth; it reserves nothing. Set it where the registry is built rather than while tasks
	///       are running, since Grow reads it without a lock.
	void SetMaxCapacity(std::size_t maxRecords) noexcept
	{
		maxCapacityRecords = maxRecords;
	}

private:
	TBank AllocateBank() noexcept;
	void DeallocateBank(TBank bank) noexcept;
	[[nodiscard]] Record& GetRecord(std::size_t recordIndex) noexcept;
	[[nodiscard]] Record* FindLiveRecord(TaskID id) noexcept;
	void ReportNoRoom(StaticString taskName) noexcept;
	void ReportRefusedGrowth(std::size_t requestedCapacity) noexcept;
};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for the task registry: identity, recycling, growth and refusal.
class TaskRegistryTest final : public TestCollection
{
public:
	TaskRegistryTest()
		: TestCollection("TaskRegistryTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
