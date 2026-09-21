// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include "RangedTask.h"
#include "ResultPacket.h"
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

private:
	/// @brief Where this task lives in the registry, and the identity every work item derived from it carries.
	/// @details Set once, when the registry issues the record. A task that was not created through a registry
	///          keeps a null ID, which no stream can resolve, so its work items are dropped rather than run.
	TaskID id;

	// Task Name
	StaticString name;

	// Number of RangedTasks
	TNumSubTasks numSubTasks;

	// Number of finished RangedTasks
	std::atomic<TNumSubTasks> numFinishedSubTasks;

	// Runnable Function
	TRunnable func;

	// Custom User Data
	void* userData;

	/// @brief This task's result, written by the task itself. See GetResult.
	ResultPacket result;

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
