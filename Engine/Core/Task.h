// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include "ResultPacket.h"
#include "Runnable.h"
#include "String/StaticString.h"
#include "TaskID.h"
#include "WorkItem.h"

namespace hbe
{
class TaskRegistry;

/// @brief Represents a unit of work to be executed by the task system. It consists of multiple
/// work items and can be split across threads.
class Task final
{
public:
	using TIndex = std::size_t;
	using TThreadID = std::thread::id;
	using TNumSubTasks = uint8_t;

	/// @brief The most subtasks a task can count, which is the width of the join counter rather than a policy choice.
	/// @note A splitter clamps to it rather than refusing, because the counter is what makes a join close, and a task
	///       that counted past it would report itself finished at the wrong item - the failure R29 exists to prevent.
	static constexpr TNumSubTasks MaxNumSubTasks = 255;

private:
	/// @brief Where this task lives in the registry, and the identity every work item derived from it carries.
	/// @details Set once, when the registry issues the record. A task that was not created through a registry
	///          keeps a null ID, which no stream can resolve, so its work items are dropped rather than run.
	TaskID id;

	// Task Name
	StaticString name;

	// Number of work items issued for this task
	TNumSubTasks numSubTasks;

	/// @brief How many work items have been handed out for this task, which is never more than numSubTasks.
	/// @details Lives in what used to be padding, so counting it costs the record nothing. It exists to make
	///          over-production catchable: a task that hands out more items than it reserved reports done before the
	///          last one runs, and a task that reports done early is the hang this subsystem has been debugged for.
	TNumSubTasks numGeneratedSubTasks;

	// Number of work items that have reported in
	std::atomic<TNumSubTasks> numFinishedSubTasks;

	// Runnable Function
	TRunnable func;

	// Custom User Data
	void* userData;

	/// @brief Notice fired if this task's work is dropped without running; `nullptr` is every task's state unless a
	///        requestor asked. Separate from `userData` above on purpose: that one belongs to the runnable, this one
	///        belongs to whoever asked to be told, and the two are not the same party.
	FAbandonedNotice abandonedNotice{ nullptr };
	void* abandonedUserData{ nullptr };

	/// @brief This task's result, written by the task itself. See GetResult.
	ResultPacket result;

public:
	Task() noexcept;
	Task(StaticString taskName, TRunnable func, void* userData) noexcept;
	~Task() = default;

	/// @brief Declare how many work items this task will be split into, before any of them is queued.
	/// @details The join is decided by counting finished items against this figure, so the figure has to be the
	///          truth before the first item can be seen by a worker. Reserving afterwards lets a task report
	///          itself done when one item of three has run, and a caller waiting on that never wakes up.
	/// @note Call it once, on a task the registry has just issued, before the first GenerateSubTask is queued.
	///       Reserving is not a promise that somebody waits: a fire-and-forget job (R16) still reserves the number
	///       of items it queues, and simply has nobody reading the join. What is optional is the reporting, not the
	///       count.
	void ReserveSubTasks(TNumSubTasks count) noexcept
	{
		numSubTasks = count;
	}

public:
	[[nodiscard]] auto GetName() const noexcept
	{
		return name;
	}

	/// @brief This task's identity. Null until a registry issues the record that holds it.
	[[nodiscard]] TaskID GetID() const noexcept
	{
		return id;
	}

	[[nodiscard]] auto NumSubTasks() const noexcept
	{
		return numSubTasks;
	}

	/// @brief The result this task writes for whoever it addressed it to.
	/// @details One packet, embedded here rather than parked in a buffer owned by a stream. It is cleared when
	///          the registry issues this record, so a task always starts from "no result" and a reused record
	///          cannot report the previous occupant's answer.
	/// @note A reader reaches it through the identity of the declaring task, so naming a task that has been
	///       released is refused by the registry instead of reading a recycled record. Nothing in the engine
	///       copies this packet, and nothing in the engine frees it.
	/// @note A task with more output than one packet holds keeps that output in memory it owns and puts a handle
	///       to it in the payload. The engine's knowledge of results stops at this packet.
	[[nodiscard]] ResultPacket& GetResult() noexcept
	{
		return result;
	}

	/// @brief Read-only view of this task's result.
	[[nodiscard]] const ResultPacket& GetResult() const noexcept
	{
		return result;
	}

	[[nodiscard]] auto NumFinishedSubTasks() const noexcept
	{
		return numFinishedSubTasks.load(std::memory_order::relaxed);
	}

private:
	friend class TaskRegistry;
	friend class WorkItem;
	friend class TaskSystem;
	friend class TaskProvider;
	friend class TaskSystemTest;
	friend class TaskRegistryTest;

	// Kept off the customer surface on purpose. This counter cannot tell a task that finished from one that was never
	// dispatched - both read as not-done here - so a customer waiting on it has no way to notice that the executor it
	// depends on is gone, and no honest deadline it could apply. A customer asks what it can answer instead: whether
	// any work is still queued, which is what OS::Logger::StopTask does. The task system needs the counter for its own
	// completion accounting, and the unit tests are white-box by construction, so the friends above are who this is
	// reachable by. A test that waits uses hbe::WaitUntil, which carries a deadline and reports a stall as a failure
	// instead of hanging.
	[[nodiscard]] bool HasDone() const noexcept
	{
		return numSubTasks > 0 && NumFinishedSubTasks() >= numSubTasks;
	}

	/// @brief Fill this task in place, as the record that holds it issues it.
	/// @details The registry owns record storage and cannot assign one task over another - the finished-subtask
	///          counter is atomic, which leaves this class without a copy or move assignment to lean on. Setting the
	///          fields is also what a recycled record needs: the previous task's identity, name, runnable and
	///          accounting must all be replaced together, before the record is published as in use.
	/// @note Reloads **every** field, including the abandonment notice. A registry record is recycled, and a notice left
	///       behind by the previous tenant of the slot would fire the old requestor's callback with its stale context
	///       pointer for work the new task never asked about - which is why the in-class initialisers on those two fields
	///       are not enough on their own.
	void LoadIntoRecord(TaskID newID, StaticString taskName, TRunnable newFunc, void* newUserData) noexcept;

public:
	/// @brief Count one finished work item, and say whether this one finished the join.
	/// @return True in exactly one caller when the last reserved item finishes, which is what makes it safe for
	///         that caller to act as the one that completed the task. The comparison is an equality rather than a
	///         threshold on purpose: past the last item every later caller would otherwise also be told it won,
	///         and a join that fires twice dispatches its successor twice.
	/// @details The counter is the join of R9, and reading it with seq_cst is what orders the task's own writes
	///          before the completion anyone else will observe.
	bool ReportFinishedSubTask() noexcept
	{
		const auto finishedBefore = numFinishedSubTasks.fetch_add(1, std::memory_order::seq_cst);

		return finishedBefore + 1 == numSubTasks;
	}

	[[nodiscard]] TRunnable GetRunnable() const noexcept
	{
		return func;
	}

	void SetRunnable(TRunnable runnable) noexcept
	{
		func = runnable;
	}

	[[nodiscard]] void* GetUserData() const noexcept
	{
		return userData;
	}

private:
	// Demoted from the customer surface, because an item is the engine's currency rather than the customer's. A
	// customer creates a task, declares its join with ReserveSubTasks and dispatches it; the items that fill the join
	// are produced and queued inside the engine, which is the only place the relationship between an item and its task
	// can be kept honest - the index range, the priority and the reserved count are engine invariants, and an item
	// built from outside them is how a task comes to report itself finished on work that never ran. The callers the
	// build can reach are TaskSystem and the unit tests, both friends above, and a provider's own contract says it
	// hands back an item built here.

	/// @brief Build one work item for the index range [start, end), which is what a queue holds.
	/// @details Does not count anything: the join is what ReserveSubTasks declared, not what has been handed out.
	///          Handing out more items than were reserved makes the task report itself finished before the last
	///          item ran, so the generated count is checked against the reservation and the excess is reported.
	WorkItem GenerateSubTask(TIndex start, TIndex end, uint8_t priority = 0) noexcept;
};
} // namespace hbe
