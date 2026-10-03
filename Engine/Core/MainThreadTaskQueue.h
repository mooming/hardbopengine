// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <functional>
#include <mutex>

#include "Container/BoundedPriorityQueue.h"


namespace hbe
{
/// @brief Thread-safe task queue for the main thread.
/// @details Workers can enqueue tasks to be executed on the main thread.
/// Uses BoundedPriorityQueue internally for efficient task management.
/// @note queueLock guards the container, which has no synchronisation of its own. BoundedPriorityQueue
///       mutates a plain size counter and its bucket vectors in both Push and Pop, so an unsynchronised
///       handoff between the enqueuing thread and the draining thread raced on that counter and on the
///       vectors - measured under ThreadSanitizer, where Push raced IsEmpty and Pop on the same object.
/// @note Task functions run with the lock released, never held across the call. A queued task may itself
///       schedule more main-thread work, and invoking it under a non-recursive mutex would deadlock the
///       moment it did.
/// @note isRunning stays outside queueLock: it is only ever touched as an atomic and guards nothing, so
///       pulling it into the critical section would widen the lock for no protection.
/// API reference: docs/Core/MainThreadTaskQueue/index.html
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
	using TQueue = BoundedPriorityQueue<TaskItem, 256, MaxQueueSize>; // hb-standards:ignore

	TQueue queue;
	/// @brief Guards queue alone. Mutable because HasPendingTasks takes it while reporting const.
	mutable std::mutex queueLock;
	std::atomic<bool> isRunning;

public:
	MainThreadTaskQueue();
	~MainThreadTaskQueue() = default;

	/// @brief Enqueue a task to be executed on the main thread.
	/// @param task The task function to execute.
	/// @param priority The priority of the task (0 = lowest, 255 = most urgent). Default is 128, the middle
	///        of the range, so a caller that states nothing sits between urgent work and background work
	///        rather than silently joining one end. Note this direction was inverted on 2026-09-18 along
	///        with BoundedPriorityQueue itself, which now drains the highest number first; anything that
	///        previously passed a small number expecting urgency now reads as low urgency.
	/// @threadsafe Callable from any thread. Window callbacks reach it through
	///             TaskSystem::DispatchToMainThread on a task-stream thread while the process main
	///             thread is draining the same queue.
	void Enqueue(TTaskFunc taskFunc, void* userData, uint8_t priority = 128) noexcept;
	/// @brief Run every queued task on the calling thread, popping under the lock and invoking outside it.
	/// @note Intended for the main thread. Returns once the queue is observed empty; producers that keep
	///       enqueueing can extend the loop, which is the behaviour callers already rely on.
	size_t ProcessTasks() noexcept;
	[[nodiscard]] bool HasPendingTasks() const noexcept;
	void RequestStop() noexcept;
	[[nodiscard]] bool IsRunning() const noexcept;
};
} // namespace hbe
