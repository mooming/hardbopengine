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
#include "Core/ResultContainer.h"
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
	/// @brief Result containers per stream: two, so one can be filled while the other is consumed.
	static constexpr std::size_t NumResultContainers = 2;

	/// @brief Slots each result container starts with: 1024, the same number for every stream.
	/// @details The arithmetic that follows from the slot width is worth stating: a slot is 128 bytes, so a
	///          container starts at 131,072 bytes and two of them cost one stream 256 KiB before any growth.
	static constexpr std::size_t InitialResultCapacitySlots = 1024;

	/// @brief Slots each result container adds when it is grown: 1024, the same number for every stream.
	/// @details Growth belongs to the stream rather than to a task, so a stream's memory ceiling is decided
	///          where the stream is built and every task running on it inherits it. A task that needs more room
	///          waits for further grows instead of asking for its own size.
	static constexpr std::size_t DefaultGrowBySlots = InitialResultCapacitySlots;

	/// @brief Most results any one task on this stream may declare: 0, which means no ceiling.
	/// @details A task declares its result count before a stream takes it on, and a declaration the stream can
	///          never satisfy must be refused while there is still a caller who can act on the refusal. This is
	///          the number it is compared against. Zero states that no declaration is too large, so nothing is
	///          ever refused for capacity and the stream grows on demand instead.
	/// @details The ceiling is not a reservation. A stream that is given one only becomes able to grow its
	///          containers that far; the memory is taken when it does. Growing is not free at this slot width -
	///          a pool serves a 131,072-byte block out of a bank of sixteen of them, 2 MiB, so a stream grown
	///          toward a high ceiling reserves in steps of 2 MiB rather than 128 KiB. That is the reason the
	///          default is the inert value rather than a conservative number.
	static constexpr std::size_t DefaultMaxResultCapacitySlots = 0;

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
	ResultContainer firstResultContainer;
	ResultContainer secondResultContainer;
	std::size_t growBySlots;
	std::size_t maxResultCapacitySlots;
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
	/// @return False if the task declared more results than this stream could ever admit, in which case it is
	///         not queued and will not run. See CanAdmitResults.
	/// @note A refused task is not re-queued, not retried and not run here. Until a closed lane grows a
	///       capacity-free fallback, honouring a refusal is the caller's job.
	[[nodiscard]] bool EnqueueFifo(const RangedTask& task) noexcept;
	/// @brief Queue a task on the priority lane, which runs the highest priority number first and the
	///        oldest first within a tie.
	/// @return False if the task declared more results than this stream could ever admit. See EnqueueFifo.
	[[nodiscard]] bool EnqueuePriority(const RangedTask& task) noexcept;
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

	/// @brief One of this stream's two result containers, by index below NumResultContainers.
	/// @details The pair exists so that filling and consuming do not have to overlap: a stream appends results
	///          to one while the other waits to be consumed. Which of the two is which is decided by the
	///          base-stream pass that swaps them, so a caller that wants a stream's results asks for the pair by
	///          index and not for a role it cannot know from outside.
	/// @note Asserts if index is not a container of this stream.
	[[nodiscard]] const ResultContainer& GetResultContainer(std::size_t index) const noexcept;

	/// @brief How many slots each of this stream's result containers adds when it is grown.
	/// @details Fixed at DefaultGrowBySlots for every stream today; a stream that needs a different figure is
	///          given one where it is built, which is also the only place its capacity could differ.
	[[nodiscard]] std::size_t GetResultGrowBy() const noexcept
	{
		return growBySlots;
	}

	/// @brief Most results any one task on this stream may declare. Zero means no ceiling.
	[[nodiscard]] std::size_t GetResultMaxCapacity() const noexcept
	{
		return maxResultCapacitySlots;
	}

	/// @brief Cap the results one task may declare on this stream. Zero lifts the cap.
	/// @note Configure before this stream is dispatched to, or from the stream's own thread. The ceiling is read
	///       by whoever enqueues, so changing it from another thread while an enqueue is in flight is a data
	///       race. It is deliberately not synchronised: the stream's own pool is not thread-safe either, so an
	///       atomic ceiling here would imply a safety the rest of the stream does not have.
	void SetResultMaxCapacity(std::size_t maxCapacitySlots) noexcept
	{
		maxResultCapacitySlots = maxCapacitySlots;
	}

	/// @brief Whether a task declaring numResults results could ever be admitted by this stream.
	/// @details A ceiling test, not an availability test: it asks whether the number is reachable at all, not
	///          whether room is free right now. A task that passes may still have to wait for the container to
	///          be grown, and a task that fails is refused now rather than accepted into a lane that would
	///          later close on it and stop serving that lane for every task on it.
	/// @note Under the default ceiling of zero every count passes, so the answer is always true and no task can
	///       be refused for capacity. That is R21's configured state, not an unimplemented check.
	[[nodiscard]] bool CanAdmitResults(Task::TNumResults numResults) const noexcept;

	void Start(TaskSystem& taskSys) noexcept;
	void RunLoop() noexcept;

private:
	void Dequeue(std::optional<RangedTask>& outTask);

	/// @brief Say that a task was refused for declaring more results than this stream could ever admit.
	/// @details Refusal is silent otherwise, and a task that never runs is indistinguishable from a task that is
	///          still waiting - the failure mode this engine has already been debugged for by watching a test
	///          stall. Naming the task, its declaration and the ceiling is what makes a configuration mistake
	///          readable from a log instead of from a hang.
	void ReportRefusal(const RangedTask& task, Task::TNumResults numResults) const noexcept;

	/// @brief Say that a queued work item was dropped because its task had been released.
	/// @details This is R7's rule in action - a reference to a task that no longer exists is recognised and dropped
	///          rather than followed - and it is warned rather than left silent because work that vanishes is the
	///          single hardest thing to diagnose in this subsystem. A caller that releases a task while its subtasks
	///          are still queued will see one line per dropped subtask.
	void ReportReleasedTask(const RangedTask& task) const noexcept;
};

} // namespace hbe
