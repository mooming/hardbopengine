// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include "Container/Array.h"
#include "Container/BoundedPriorityQueue.h"
#include "Container/Deque.h"
#include "Core/CPUBudget.h"
#include "Core/MainThreadTaskQueue.h"
#include "Core/StreamDrainPolicy.h"
#include "HSTL/HVector.h"
#include "Memory/MultiPoolAllocator.h"
#include "String/StaticString.h"
#include "Task.h"
#include "TaskStreamIndex.h"
#include "WorkItem.h"

namespace hbe
{
class TaskProvider;

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
	using TWorkItems = TVector<WorkItem>;

private:
	struct TaskQueueItem final
	{
		uint8_t priority;
		mutable WorkItem task;
		float duration;

		TaskQueueItem(uint8_t priority, const WorkItem& task);

		TaskQueueItem& operator=(const TaskQueueItem& other) = default;
		bool operator<(const TaskQueueItem& other) const;
	};

	/// @brief Providers handing this stream work, one list per lane.
	/// @details The stream's list, not the provider's, is what the drain reads: a provider's own attachment slots
	/// record
	///          what its user asked for, and a stream only ever asks a provider that registered with it (R38). A
	///          provider attached to a stream the engine does not have is therefore recorded and never asked, which the
	///          provider reports rather than hides.
	/// @brief How many providers may feed one lane of one stream.
	/// @details The realistic figure is one, and a lane is asked at most once per pass per provider registered on it,
	/// so
	///          this is a bound to keep the list inline rather than a limit anyone is expected to reach. Overflow is
	///          refused by `AttachProvider` and reported by the provider that asked.
	static constexpr TIndex MaxProvidersPerLane = 8;

	struct LaneProviders final
	{
		std::array<TaskProvider*, static_cast<size_t>(MaxProvidersPerLane)> items{};
		TIndex count = 0;
	};

	std::array<LaneProviders, 2> laneProviders;

	StaticString name;
	TThreadID threadID;
	TStreamIndex streamIndex;
	std::uint64_t loopCount;
	MultiPoolAllocator allocator;

	/// @brief The task system this stream works for, recorded rather than looked up.
	/// @details `Update` may run on a thread that outlives the engine, so it cannot resolve the task system through the
	///          live engine the way a thread-owned loop can.
	TaskSystem* taskSystem = nullptr;

	HVector<WorkItem> readdingFifo;
	HVector<WorkItem> readdingPriority;
	CPUBudget budget;

	std::mutex queueLock;
	std::condition_variable cv;
	std::thread thread;
	Deque<WorkItem> fifoQueue;
	BoundedPriorityQueue<WorkItem> priorityQueue;
	StreamDrainPolicy drainPolicy;

	/// @brief Whether the budget window has closed since this stream last reopened. Set by the base stream's
	///        pass, cleared by this stream's own thread.
	/// @details The pass signals and this stream reopens, which is what keeps every write to the budget on the
	///          thread whose CPU it charges - see RequestWindowAdvance.
	std::atomic<bool> windowAdvanceRequested{false};

	/// @brief Set when this stream should finish what it holds and stop, independently of every other stream.
	std::atomic<bool> closeRequested{false};

	/// @brief Set once this stream has drained its work and left its loop, or been closed from outside.
	std::atomic<bool> isClosed{false};

	/// @brief How many passes this stream has run. Written by the single thread driving this stream, so it needs no
	/// atomic.
	std::uint64_t drivenPassCount = 0;

	/// @brief Work posted to the thread this stream drives, held here so that one pump reaches it.
	/// @details A callable posted from any thread is a promise that the engine loop will run it. It lives on the stream
	/// that
	///          the engine loop drives, rather than beside the stream, because that is what makes the stream's own pump
	///          the only place work is taken: a wait that drives the stream then cannot fail to reach posted work,
	///          which was a real deadlock and not a hypothetical one. Thread-safe on its own - the enqueuer is
	///          arbitrary, the drainer is this stream's driver, and the queue invokes callables with its lock released.
	MainThreadTaskQueue postedTasks;

	bool isPumping = false;

	bool nestedPumpAllowed = false;

	/// @brief Set while this stream runs its own remaining work on the way out, which suspends provider probing.
	bool isDrainingForShutdown = false;

	/// @brief How long this stream's shutdown drain may spend before remaining work is reported as abandoned.
	/// @details Worker streams get a real window. The engine's own streams get none, because "run until the queues are
	/// empty"
	///          is not what closing them means: the IO stream's work is the log buffer, and writing a line re-posts the
	///          drain task, so draining it to emptiness is a loop that ends only at the deadline - measured at over 2.6
	///          million passes and the full window for no work but its own. The shutdown flushes the logger explicitly
	///          instead, which is that stream's real work, and the base stream's end-of-run duties are likewise driven
	///          by the shutdown rather than discovered by a drain.
	std::chrono::milliseconds shutdownDrainDeadline{2000};
	/// @brief Times this stream left a task in the general queue because its allowance was already spent.
	/// @details The only witness that the acquire gate exists: a stream that declines work is otherwise
	///          indistinguishable from one that has nothing to do.
	std::atomic<unsigned> generalQueueRefusals{0};

	/// @brief Times a pass found work waiting in a lane and chose no lane, which is the lane throttle itself.
	/// @details StreamDrainPolicy keeps its own per-lane time book and returns no lane once that is spent,
	/// independently of
	///          CPUBudget, so this counts the lane gate while GetGeneralQueueRefusalCount counts the CPUBudget gate.
	///          Both are needed: the engine throttles from two books, and a watch that reads only one cannot see work
	///          held by the other.
	std::atomic<unsigned> laneWorkRefusals{0};

	/// @brief Times a provider was asked for work while this stream's allowance was already spent.
	/// @details The invariant, made countable so it holds in a Release build too, where the assert on the provider gate
	/// is
	///          compiled out. It must stay zero: a spent stream that still manufactures work will run work it has no
	///          right to.
	std::atomic<unsigned> providerAsksWhileSpent{0};

public:
	TaskStream();
	explicit TaskStream(StaticString name, TStreamIndex streamIndex);
	~TaskStream() = default;

	/// @brief Queue a task on the FIFO lane, which runs them in arrival order.
	/// @details Cannot fail. A task is queued or it is not, and the only reason a stream ever refused one was a
	///          declared result count it could not fit - a question that stopped existing when the result moved
	///          into the task's own record.
	void EnqueueFifo(const WorkItem& task) noexcept;

	/// @brief Queue a task on the priority lane, which runs the highest priority number first and the
	///        oldest first within a tie.
	void EnqueuePriority(const WorkItem& task) noexcept;

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

	/// @brief Ask this stream to change its CPU allowance, from any thread.
	/// @details `ConfigureBudget` writes a field the stream reads on its own thread every pass, so from elsewhere it is
	///          a data race, not a late write. This stores one pending value atomically and the stream applies it to
	///          itself at the top of its next loop, which is the hand-off the accounting window already uses (the pass
	///          signals, the stream reopens). Consequence for callers: the change is not instantaneous - it lands on
	///          the stream's next pass, and the most recent request wins over any earlier unapplied one.
	/// @note The drain policy's allowance follows the same apply, because a lane ratio computed against the old
	///       allowance would spread work against a figure the stream no longer has.
	void RequestBudget(std::chrono::duration<double> allowance) noexcept;

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
	/// @note Wired today in front of the general queue only, and not in front of a stream's own lanes. An entry on
	///       a lane is either a task nobody has taken or one taken and paused before it finished, and the two are
	///       indistinguishable without a field the work item does not have. Refusing the lane would strand the
	///       second kind, which is the failure this gate exists not to cause.
	[[nodiscard]] bool MayTakeNewWork() const noexcept;

	/// @brief CPU time this stream has charged to its budget so far.
	/// @threadsafe Readable from any thread, though it is written only by the stream's own thread; reading
	///             it elsewhere sees the value as of the last task that thread finished.
	[[nodiscard]] std::chrono::nanoseconds GetAccumulatedCPUTime() const noexcept;

	/// @brief Mark that the budget window has closed, so this stream reopens its allowance on its next pass
	///        round the loop.
	/// @details Signals only: it writes this one atomic and no budget field. A budget charges the CPU time of the
	///          thread that owns it, so a reset executed on another thread lands on the pairing between that
	///          thread's BeginTask and EndTask, and the cost of breaking the pairing is either a lost charge or
	///          the thread's whole life billed to the budget, which reads as a stream that refuses work forever.
	///          A reopen that cannot interrupt a task in flight is the honest shape: it means a window boundary
	///          falls between tasks, which is the overshoot rule MayTakeNewWork already states.
	/// @threadsafe Callable from any thread. The base stream's pass is the only caller in the engine.
	void RequestWindowAdvance() noexcept
	{
		windowAdvanceRequested.store(true, std::memory_order_relaxed);
	}

	/// @brief Times this stream declined to take a task from the general queue because its allowance was spent.
	/// @threadsafe Readable from any thread.
	/// @brief How many passes declined a lane that held work because the drain policy's allowance was spent.
	[[nodiscard]] unsigned GetLaneWorkRefusalCount() const noexcept
	{
		return laneWorkRefusals.load(std::memory_order_relaxed);
	}

	/// @brief How many times a provider was asked while the allowance was spent. Zero unless the gate is broken.
	[[nodiscard]] unsigned GetProviderAskWhileSpentCount() const noexcept
	{
		return providerAsksWhileSpent.load(std::memory_order_relaxed);
	}

	[[nodiscard]] unsigned GetGeneralQueueRefusalCount() const noexcept
	{
		return generalQueueRefusals.load(std::memory_order_relaxed);
	}

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

	/// @brief Let a provider hand this stream work on one lane.
	/// @details Called by `TaskProvider::AttachTo` for a stream the engine really has; the provider's own slot list is
	///          bookkeeping, and this list is what the drain reads (R38). Registering the same provider on the same
	///          lane twice is refused, because the drain walks the list and would ask it twice per pass.
	/// @return False if the provider is already registered on this lane.
	bool AttachProvider(TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept;

	/// @brief Stop a provider from feeding one lane, and report whether it was registered there.
	/// @details Takes this stream's queue lock, which is the same lock a drain holds across `Produce` - so a detach
	/// that
	///          arrives mid-call simply waits, and there is no window where a provider is erased while its item is on
	///          the way. That is R37's guarantee, and R40 is the reason it costs no counter or wait loop: the lock is
	///          the wait.
	bool DetachProvider(TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept;

	/// @brief Whether a provider is registered on one lane of this stream.
	[[nodiscard]] bool IsProviderAttached(const TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept;

	void Start(TaskSystem& taskSys) noexcept;

	/// @brief Drive exactly one pass of this stream, on whatever thread calls it.
	/// @details The pass is non-blocking: it sweeps finished work, applies a pending allowance request, reopens the
	///          accounting window if one was asked for, takes at most one item from a lane, asks the providers attached
	///          to the empty lanes, and otherwise falls back to the general queue. It returns true when it took or
	///          dropped something, so a driver that has other duties knows whether to keep going or to call
	///          `WaitForWork`.
	/// @note One driver at a time, and that driver is this stream's owner thread for every purpose that asks who owns
	/// it:
	///       `IsBaseThread`, `IsIOThread`, the owner-thread budget asserts, and the thread-local stream index. The
	///       index and the allocator scope are entered and restored around each pass, because a shared driver does not
	///       belong to this stream for the whole of its life.
	/// @note This resolves nothing global. A driven stream must outlive the engine object that hosts the task system,
	/// so the
	///       pass reads the task system recorded at registration and never the live engine.
	bool Update() noexcept;

	/// @brief Ask this stream to finish its work and close.
	/// @details The counterpart to a shutdown request. One global flag stops every stream in the same instant, which
	/// abandons
	///          whatever each of them already holds and closes the base stream along with the rest even though that
	///          stream still owns the end of the run. Closing is therefore per stream and ordered by whoever drives the
	///          shutdown: everything else first, the base stream last.
	void RequestClose() noexcept
	{
		closeRequested.store(true, std::memory_order_release);
		cv.notify_all();
	}

	/// @brief Permit or forbid a nested pump from inside work this stream is already running.
	/// @details Only a designated wait point may allow re-entry, and it withdraws the permission on the way out.
	void SetNestedPumpAllowed(bool allowed) noexcept;

	/// @brief Post a callable to be run by the thread driving this stream, from any thread.
	/// @param taskFunc Invoked with userData on the driving thread. Must not be null.
	/// @param userData Passed through untouched. May be null.
	/// @param priority 0 is the least urgent, 255 the most. The default is 128, matching a plain engine-loop post.
	/// @note Cannot refuse the work. The queue is bounded; a post past the bound is a programming error and reports
	/// itself.
	void DispatchPostedTasks(MainThreadTaskQueue::TTaskFunc taskFunc, void* userData, uint8_t priority = 128) noexcept;

	/// @brief Run every callable posted to this stream that is runnable now, and say how many ran.
	/// @details Called from the stream's own pump, so a thread driving the stream reaches posted work without knowing
	/// it exists.
	size_t ProcessPostedTasks() noexcept;

	/// @brief Whether any callable is waiting to be run by this stream's driver.
	/// @note A snapshot across another thread's queue: a false answer of "none" is only sound once the poster has
	/// stopped.
	[[nodiscard]] bool HasPostedTasks() const noexcept;

	/// @brief Whether this stream has been asked to close.
	[[nodiscard]] bool IsCloseRequested() const noexcept
	{
		return closeRequested.load(std::memory_order_acquire);
	}

	/// @brief Whether this stream has drained its own work and left its loop.
	/// @details Being asked to close is not being closed: the ask is a request and this says the stream finished.
	/// Whoever
	///          orders the shutdown waits on this rather than on a thread handle, because a driven stream has no thread
	///          to join.
	/// @brief Close this stream from the outside, for a stream that has no thread of its own.
	/// @details A ride-on stream never enters `RunLoop`, so nothing there would drain it, report it, or mark it closed
	/// - and a
	///          stream whose closed state is never set is a stream that any correct future waiter would wait on
	///          forever. Whoever drove it closes it, after the driver has been withdrawn and can no longer be inside a
	///          pass.
	void CloseDrivenStream() noexcept;

	/// @brief Number of `Update` passes this stream has run, on whichever thread drove it.
	[[nodiscard]] std::uint64_t GetDrivenPassCount() const noexcept
	{
		return drivenPassCount;
	}

	[[nodiscard]] bool IsClosed() const noexcept
	{
		return isClosed.load(std::memory_order_acquire);
	}

	/// @brief Run this stream's own remaining work, then stop.
	/// @details The graceful half of closing: a stream that was asked to close keeps taking and running what it already
	/// holds
	///          instead of handing items back to nobody and destroying its re-add buffers with work inside them.
	///          Provider probing is suspended, because a provider can answer every request with another item and a
	///          drain that never ends is not a close. The bound is a wall clock rather than a pass count, because a
	///          resumable task is re-added on every pass and would outrun any pass limit - a bound that such work can
	///          defeat is not a bound.
	/// @note Work still held when the deadline passes is **reported as abandoned** rather than looped over forever. A
	/// task that
	///       can never finish is a defect in that task, and the shutdown's job is to say so and proceed.
	/// @return The number of passes taken; abandoned items are reported by this call directly.
	std::uint64_t DrainForShutdown() noexcept;

	/// @brief How many items this stream still holds in its lanes.
	[[nodiscard]] std::size_t CountPendingItems() const noexcept;

	/// @brief Sleep until this stream is worth driving again, or `patience` runs out.
	/// @details Separate from `Update` on purpose: a pass that found nothing must not decide how long to wait, because
	/// a
	///          driver with several duties knows its own latency budget and this stream cannot.
	void WaitForWork(std::chrono::milliseconds patience) noexcept;
	void RunLoop() noexcept;

private:
	void Dequeue(std::optional<WorkItem>& outTask);

	/// @brief Ask the providers on one lane for an item, when that lane has run out of queued work.
	/// @details The drain position R39 settled: a lane empty of items asks once per pass, under the same budget gate
	/// that
	///          governs taking queued work - a stream whose allowance is spent must not manufacture work the budget
	///          already refused. Providers are asked in registration order and the first that hands something back
	///          wins; a provider with nothing returns `nullopt` and the lane stops being drained this pass rather than
	///          spinning.
	/// @note The queue lock is held across the provider call, so this helper never runs user code unlocked and never
	///       re-enters the queue. A provider must therefore not touch this stream from inside `Produce`.
	std::optional<WorkItem> DrainProvidersLocked(StreamDrainPolicy::ELane lane) noexcept;

	/// @brief Say that a queued work item was dropped because its task had been released.
	/// @details This is R7's rule in action - a reference to a task that no longer exists is recognised and dropped
	///          rather than followed - and it is warned rather than left silent because work that vanishes is the
	///          single hardest thing to diagnose in this subsystem. A caller that releases a task while its subtasks
	///          are still queued will see one line per dropped subtask.
	void ReportReleasedTask(const WorkItem& task) const noexcept;

	/// @brief Count a refusal to take from the general queue, and report the first one this stream ever refuses.
	/// @details A throttled stream has to be visible somewhere or it looks like an idle one, and the count alone
	///          is invisible to anyone not reading a debugger.
	void ReportGeneralQueueRefusal() noexcept;
};

} // namespace hbe
