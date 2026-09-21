// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include "Container/Array.h"
#include "Container/BoundedPriorityQueue.h"
#include "Container/Deque.h"
#include "Core/CPUBudget.h"
#include "Core/StreamDrainPolicy.h"
#include "HSTL/HVector.h"
#include "Memory/MultiPoolAllocator.h"
#include "RangedTask.h"
#include "String/StaticString.h"
#include "Task.h"
#include "TaskStreamIndex.h"

namespace hbe
{
class TaskStream;
class TaskSystem;

/// @brief Represents a thread that processes a series of tasks from a priority queue.
class TaskStream final
{
public:
private:
	template <typename T>
	using TVector = hbe::HVector<T>;
	using TIndex = Task::TIndex;
	using TStreamIndex = hbe::TStreamIndex;
	using TThreadID = std::thread::id;
	using TRangedTasks = TVector<RangedTask>;

private:
	struct TaskQueueItem final
	{
		uint8_t priority;
		mutable RangedTask task;
		float duration;

		TaskQueueItem(uint8_t priority, const RangedTask& task);

		TaskQueueItem& operator=(const TaskQueueItem& other) = default;
		bool operator<(const TaskQueueItem& other) const;
	};

	StaticString name;
	TThreadID threadID;
	TStreamIndex streamIndex;
	std::uint64_t loopCount;
	MultiPoolAllocator allocator;
	CPUBudget budget;

	std::mutex queueLock;
	std::condition_variable cv;
	std::thread thread;
	Deque<RangedTask> fifoQueue;
	BoundedPriorityQueue<RangedTask> priorityQueue;
	StreamDrainPolicy drainPolicy;

public:
	TaskStream();
	explicit TaskStream(StaticString name, TStreamIndex streamIndex);
	~TaskStream() = default;

	/// @brief Queue a task on the FIFO lane, which runs them in arrival order.
	/// @details Cannot fail. A task is queued or it is not, and the only reason a stream ever refused one was a
	///          declared result count it could not fit - a question that stopped existing when the result moved
	///          into the task's own record.
	void EnqueueFifo(const RangedTask& task) noexcept;

	/// @brief Queue a task on the priority lane, which runs the highest priority number first and the
	///        oldest first within a tie.
	void EnqueuePriority(const RangedTask& task) noexcept;

	/// @brief Set this stream's FIFO:priority rate, which is a share of its CPU allowance. Zero weights are
	///        treated as one, not as "never serve this lane".
	void ConfigureRate(uint32_t fifoWeight, uint32_t priorityWeight) noexcept;
	void WakeUp() noexcept;

	/// @brief Set this stream's CPU allowance, expressed as a duration. Zero means unlimited.
	/// @details The yardstick it is measured against is time::GetBaseFramePeriod, engine-wide, so a caller
	///          states how much CPU this stream may spend per frame period of the base stream and leaves the
	///          arithmetic alone.
	/// @note A stream with no allowance does not measure its tasks at all. A measurement that cannot change a
	///       decision is a syscall billed to every task for nothing, and an unlimited budget cannot change a
	///       decision. Consequently GetAccumulatedCPUTime stays zero until an allowance is set - that is the
	///       absence of accounting, not a task that cost nothing.
	/// @note Configure before the streams start, or from the stream's own thread. The allowance is not
	///       synchronised against the stream thread, which reads it once per task to decide whether to
	///       measure at all; a mid-run change from elsewhere is a data race, not merely a late one.
	void ConfigureBudget(std::chrono::duration<double> allowance) noexcept;

	/// @brief Whether this stream may take on another task.
	/// @details The gate belongs in front of work being ACQUIRED - draining a provider, dequeuing from a
	///          feeder - and never in front of running a task already held or delivering a result. Refusing
	///          to dequeue is the mechanism; refusing to finish work that is in flight would turn a budget
	///          into a way to strand results, which is the failure this whole design is careful not to have.
	/// @details True while the allowance is unspent, and always true for an unlimited stream. A task already
	///          running overshoots rather than being truncated, because a task cannot be stopped once taken:
	///          the overshoot is bounded by the longest task, not by the allowance.
	/// @threadsafe Readable from any thread. Enquiry does not act on the answer, so a reader that acts must
	///             accept that another thread may have spent the budget in between.
	[[nodiscard]] bool MayTakeNewWork() const noexcept;

	/// @brief CPU time this stream has charged to its budget so far.
	/// @threadsafe Readable from any thread, though it is written only by the stream's own thread; reading
	///             it elsewhere sees the value as of the last task that thread finished.
	[[nodiscard]] std::chrono::nanoseconds GetAccumulatedCPUTime() const noexcept;

	void Join() noexcept
	{
		thread.join();
	}

	[[nodiscard]] auto GetName() const noexcept
	{
		return name;
	}

	[[nodiscard]] auto GetThreadID() const noexcept
	{
		return threadID;
	}

	[[nodiscard]] auto& GetThread() noexcept
	{
		return thread;
	}

	[[nodiscard]] auto& GetThread() const noexcept
	{
		return thread;
	}

	[[nodiscard]] auto GetStreamIndex() const noexcept
	{
		return streamIndex;
	}

	[[nodiscard]] auto GetLoopCount() const noexcept
	{
		return loopCount;
	}

	void Start(TaskSystem& taskSys) noexcept;
	void RunLoop() noexcept;

private:
	void Dequeue(std::optional<RangedTask>& outTask);

	/// @brief Say that a queued work item was dropped because its task had been released.
	/// @details This is R7's rule in action - a reference to a task that no longer exists is recognised and dropped
	///          rather than followed - and it is warned rather than left silent because work that vanishes is the
	///          single hardest thing to diagnose in this subsystem. A caller that releases a task while its subtasks
	///          are still queued will see one line per dropped subtask.
	void ReportReleasedTask(const RangedTask& task) const noexcept;
};

} // namespace hbe
