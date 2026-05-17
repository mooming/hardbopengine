// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <queue>
#include "Container/Array.h"
#include "Container/BoundedPriorityQueue.h"
#include "HSTL/HVector.h"
#include "Memory/MultiPoolAllocator.h"
#include "RangedTask.h"
#include "String/StaticString.h"
#include "Task.h"

namespace hbe
{
class TaskStream;
class TaskSystem;

/// @brief Represents a thread that processes a series of tasks from a priority queue.
class TaskStream final
{
	template<typename T>
	using TVector = hbe::HVector<T>;
	using TIndex = Task::TIndex;
	using TStreamIndex = Array<RangedTask>::TIndex;
	using TThreadID = std::thread::id;
	using TRangedTasks = TVector<RangedTask>;

private:
	struct TaskQueueItem final
	{
		uint8_t priority;
		mutable RangedTask task;
		float duration;

		TaskQueueItem(uint8_t priority, const RangedTask& task);

		TaskQueueItem& operator= (const TaskQueueItem& other) = default;
		bool operator< (const TaskQueueItem& other) const;
	};

	StaticString name;
	TThreadID threadID;
	TStreamIndex streamIndex;
	std::uint64_t loopCount;
	MultiPoolAllocator allocator;

	std::mutex queueLock;
	std::condition_variable cv;
	std::thread thread;
	BoundedPriorityQueue<RangedTask> taskQueue;

public:
	TaskStream();
	explicit TaskStream(StaticString name, TStreamIndex streamIndex);
	~TaskStream() = default;

	void Enqueue(const RangedTask& task) noexcept;
	void WakeUp() noexcept;
	void Join() noexcept { thread.join(); }

	[[nodiscard]] auto GetName() const noexcept { return name; }
	[[nodiscard]] auto GetThreadID() const noexcept { return threadID; }
	[[nodiscard]] auto& GetThread() noexcept { return thread; }
	[[nodiscard]] auto& GetThread() const noexcept { return thread; }
	[[nodiscard]] auto GetStreamIndex() const noexcept { return streamIndex; }
	[[nodiscard]] auto GetLoopCount() const noexcept { return loopCount; }

	void Start(TaskSystem& taskSys) noexcept;
	void RunLoop() noexcept;

private:
	void Dequeue(std::optional<RangedTask>& outTask);
};

} // namespace hbe
