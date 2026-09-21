// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.
// Created by mooming on 11/10/2025.

#pragma once
#include <cstddef>
#include "String/StaticString.h"
#include "TaskID.h"
#include "TaskStreamAffinity.h"

namespace hbe
{
class Task;

/// @brief A task with a specific range of indices [start, end) that can be executed in parallel.
class RangedTask final
{
	using TIndex = std::size_t;

public:
	uint8_t priority;
	mutable TaskStreamAffinity affinity;
	StaticString taskName;

	/// @brief Which task this work item belongs to, as the task registry issued it.
	/// @details Not a reference to the task. A work item can sit in a queue long after the object it was made from
	///          has stopped meaning anything, and an ID is what lets the stream notice that and drop the work
	///          rather than run it through a dead task's fields.
	TaskID taskID;


	// RangedTask Start Index
	TIndex start;

	// RangedTask End Index
	TIndex end;

	// Index to be processed
	mutable TIndex currentIndex;

public:
	~RangedTask() = default;
	RangedTask& operator=(const RangedTask& other) = default;

	bool operator<(const RangedTask& other) const noexcept
	{
		return priority < other.priority;
	}

	[[nodiscard]] bool HasFinished() const noexcept
	{
		return currentIndex >= end;
	}

	/// @brief Run this item's slice of the task's work and report whether it was the last one.
	/// @param task The task this item was issued for, resolved by the stream from TaskID before calling.
	/// @note The caller has already checked that the ID names a live task. A work item whose task has been released
	///       is dropped by whoever dequeued it; nothing here reaches for a task by itself.
	/// @return True when this call closed the task's join, meaning every reserved subtask has now reported in and the
	///         outcome, if any, is ready to route. Exactly one item of a task ever gets true, and under R29 that item
	///         is the last reserved one rather than merely one of the ones past the count.
	/// @note The caller that gets true is the thread that finished the work, and it owes the successor a dispatch:
	///       TaskSystem::DispatchSuccessor. Nothing else delivers an outcome.
	bool Run(Task& task) noexcept;

private:
	RangedTask(Task& task, TIndex start, TIndex end, uint8_t priority) noexcept;

	friend class Task;
	friend class TaskStream;
};
} // namespace hbe
