// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


#include <atomic>
#include <functional>
#include "Container/BoundedPriorityQueue.h"

namespace hbe
{

/// @brief Thread-safe task queue for the main thread.
/// @details Workers can enqueue tasks to be executed on the main thread.
/// Uses BoundedPriorityQueue internally for efficient task management.
class MainThreadTaskQueue final
{
public:
using TTaskFunc = void (*)(void*);

private:
struct TaskItem
{
	uint8_t priority;
	mutable bool isDone;
	TTaskFunc taskFunc;
	void* userData;

	TaskItem(uint8_t p, TTaskFunc t, void* userData)
		: priority(p)
		, isDone(false)
		, taskFunc(t)
		, userData(userData)
	{
	}

	bool operator<(const TaskItem& other) const noexcept
	{
		return priority < other.priority;
	}

	[[nodiscard]] bool HasFinished() const noexcept
	{
		return isDone;
	}
};

static constexpr size_t MaxQueueSize = 1024;
using TQueue = BoundedPriorityQueue<TaskItem, 256, MaxQueueSize>;

TQueue queue;
std::atomic<bool> isRunning;

public:
MainThreadTaskQueue();
~MainThreadTaskQueue() = default;

/// @brief Enqueue a task to be executed on the main thread.
/// @param task The task function to execute.
/// @param priority The priority of the task (0 = highest, 255 = lowest). Default is 128.
void Enqueue(TTaskFunc taskFunc, void* userData, uint8_t priority = 128) noexcept;
size_t ProcessTasks() noexcept;
[[nodiscard]] bool HasPendingTasks() const noexcept;
void RequestStop() noexcept;
[[nodiscard]] bool IsRunning() const noexcept;
};

} // namespace hbe
