// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include "RangedTask.h"
#include "Runnable.h"
#include "String/StaticString.h"
#include "TaskID.h"

namespace hbe
{
class TaskRegistry;

/// @brief Represents a unit of work to be executed by the task system. It consists of multiple
/// RangedTasks and can be split across threads.
class Task final
{
public:
	using TIndex = std::size_t;
	using TThreadID = std::thread::id;
	using TNumSubTasks = uint8_t;
	using TNumResults = std::uint32_t;

private:
	/// @brief Where this task lives in the registry, and the identity every work item derived from it carries.
	/// @details Set once, when the registry issues the record. A task that was not created through a registry
	///          keeps a null ID, which no stream can resolve, so its work items are dropped rather than run.
	TaskID id;

	// Task Name
	StaticString name;

	/// @brief Result packets this task declares it can produce. See GetNumResults.
	TNumResults numResults;

	// Number of RangedTasks
	TNumSubTasks numSubTasks;

	// Number of finished RangedTasks
	std::atomic<TNumSubTasks> numFinishedSubTasks;

	// Runnable Function
	TRunnable func;

	// Custom User Data
	void* userData;

public:
	Task() noexcept;
	Task(StaticString taskName, TRunnable func, void* userData) noexcept;
	~Task() = default;

	void Start(TIndex numberOfSubTasks, TIndex startIndex, TIndex endIndex, uint8_t priority = 0) noexcept;

	// Wait
	void BusyWait() const noexcept;
	void Wait(uint32_t intervalMilliSecs = 10) const noexcept;

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

	/// @brief How many result packets this task declares it can produce. Zero is fire-and-forget.
	/// @details The declaration is what a stream admits work against: room for the declared count is meant to
	///          be secured before the task is taken on, so a task producing more than it declared is a
	///          programming error rather than a runtime condition anyone handles. This integer is also the whole
	///          of completion - above zero the task reports completion, at zero it produces nothing at the end
	///          of its run - which is why there is no separate "reports completion" flag for a caller to
	///          contradict by setting both halves at once.
	/// @note Independent of the subtask accounting in this class: NumSubTasks, NumFinishedSubTasks and HasDone
	///       describe ranged subtasks and are unchanged by anything here. A task declaring zero results still
	///       reports finished subtasks exactly as it does today.
	/// @note Nothing admits or refuses on this count yet. A stream checks declared capacity against its result
	///       containers when the capacity-admission step lands; until then the declaration is stored and read
	///       back, and no task is blocked by it.
	[[nodiscard]] TNumResults GetNumResults() const noexcept
	{
		return numResults;
	}

	/// @brief Declare how many result packets this task can produce. Zero, which is the default, is
	///        fire-and-forget.
	/// @note Declare it before dispatching the task. A stream reads the count when it decides whether to take
	///       the task on, so a later change is a different answer to a question that has already been asked.
	void SetNumResults(TNumResults inNumResults) noexcept
	{
		numResults = inNumResults;
	}

	[[nodiscard]] auto NumFinishedSubTasks() const noexcept
	{
		return numFinishedSubTasks.load(std::memory_order::relaxed);
	}

	[[nodiscard]] bool HasDone() const noexcept
	{
		return numSubTasks > 0 && NumFinishedSubTasks() >= numSubTasks;
	}

private:
	friend class TaskRegistry;

	/// @brief Fill this task in place, as the record that holds it issues it.
	/// @details The registry owns record storage and cannot assign one task over another - the finished-subtask
	///          counter is atomic, which leaves this class without a copy or move assignment to lean on. Setting the
	///          fields is also what a recycled record needs: the previous task's identity, name, runnable and
	///          accounting must all be replaced together, before the record is published as in use.
	void LoadIntoRecord(TaskID newID, StaticString taskName, TRunnable newFunc, void* newUserData) noexcept;

public:
	// Increase numFinishedSubTasks.  It guarantees all other global memory values are synced properly.
	void ReportFinishedSubTask() noexcept
	{
		numFinishedSubTasks.fetch_add(1, std::memory_order::seq_cst);
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

	// Generate a RangedTask with the given range [start, end)
	RangedTask GenerateSubTask(TIndex start, TIndex end, uint8_t priority = 0) noexcept;
};
} // namespace hbe
