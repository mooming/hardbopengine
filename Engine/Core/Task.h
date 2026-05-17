// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <thread>
#include "RangedTask.h"
#include "Runnable.h"
#include "String/StaticString.h"

namespace hbe
{
/// @brief Represents a unit of work to be executed by the task system. It consists of multiple
/// RangedTasks and can be split across threads.
class Task final
{
public:
	using TIndex = std::size_t;
	using TThreadID = std::thread::id;
	using TNumSubTasks = uint8_t;

private:
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

public:
	Task() noexcept;
	Task(StaticString taskName, TRunnable func, void* userData) noexcept;
	~Task() = default;

	void Start(TIndex numberOfSubTasks, TIndex startIndex, TIndex endIndex, uint8_t priority = 0) noexcept;

	// Wait
	void BusyWait() const noexcept;
	void Wait(uint32_t intervalMilliSecs = 10) const noexcept;

public:
	[[nodiscard]] auto GetName() const noexcept { return name; }
	[[nodiscard]] auto NumSubTasks() const noexcept { return numSubTasks; }
	[[nodiscard]] auto NumFinishedSubTasks() const noexcept { return numFinishedSubTasks.load(std::memory_order::relaxed); }
	[[nodiscard]] bool HasDone() const noexcept { return numSubTasks > 0 && NumFinishedSubTasks() >= numSubTasks; }

	// Increase numFinishedSubTasks.  It guarantees all other global memory values are synced properly.
	void ReportFinishedSubTask() noexcept { numFinishedSubTasks.fetch_add(1, std::memory_order::seq_cst); }

	[[nodiscard]] TRunnable GetRunnable() const noexcept { return func;}
	void SetRunnable(TRunnable runnable) noexcept { func = runnable; }
	[[nodiscard]] void* GetUserData() const noexcept { return userData; }

	// Generate a RangedTask with the given range [start, end)
	RangedTask GenerateSubTask(TIndex start, TIndex end, uint8_t priority = 0) noexcept;
};
} // namespace hbe
