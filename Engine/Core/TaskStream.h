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
		using ThreadID = std::thread::id;
		using RangedTasks = TVector<RangedTask>;

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
		ThreadID threadID;
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

		void Enqueue(const RangedTask& task);
		void WakeUp();
		void Join() { thread.join(); }

		[[nodiscard]] auto GetName() const { return name; }
		[[nodiscard]] auto GetThreadID() const { return threadID; }
		[[nodiscard]] auto& GetThread() { return thread; }
		[[nodiscard]] auto& GetThread() const { return thread; }
		[[nodiscard]] auto GetStreamIndex() const { return streamIndex; }
		[[nodiscard]] auto GetLoopCount() const { return loopCount; }

		void Start(TaskSystem& taskSys);
		void RunLoop();

	private:
		void Dequeue(std::optional<RangedTask>& outTask);
	};

} // namespace hbe
