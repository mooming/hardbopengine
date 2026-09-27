// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


#include <atomic>
#include <thread>

#include "Container/Array.h"
#include "Container/BoundedPriorityQueue.h"
#include "TaskRegistry.h"
#include "TaskStream.h"
#include "Time.h"

namespace hbe
{

/// @brief Manages tasks and task streams for parallel task execution.
class TaskSystem final
{
public:
	using TThread = std::thread;
	using TThreadID = std::thread::id;
	using TStreamArray = Array<TaskStream>;
	using TIndex = TStreamArray::TIndex;
	using TMainThreadTask = void (*)(void* /*userData*/);

	/// @brief Stream index of a thread that has not been given a stream, which is what every thread reports
	///        until a TaskStream loop claims it.
	/// @details Not zero: stream 0 is a real stream, so a default of zero made every thread in the process -
	///          including any the application creates, and the thread that drives Engine::Run before it claims a
	///          stream - report itself the base stream. That made IsBaseThread useless as a check, made the assert
	///          in BuildStreams pass on any thread at all, and made the general queue charge a non-stream
	///          thread's contact with a task to stream 0's affinity.
	/// @note TaskStreamAffinity drops bit indices at or above its width, so an index with this value is not
	///       recorded anywhere: a thread that is not a stream takes nothing from the general queue.
	static constexpr TIndex NonStreamIndex = static_cast<TIndex>(-1);

	/// @brief The OS thread name of the thread that drives Engine::Run and shuts the engine down.
	/// @note Distinct from the base stream, which is stream 0 and runs on its own thread. The two were both
	///       called "base" and both named "Base", so a log line or a debugger could not tell them apart.
	static constexpr const char* EngineLoopThreadName = "EngineLoop";

	static constexpr TIndex BaseStreamIndex = 0;
	static constexpr TIndex IOStreamIndex = 1;

private:
	std::atomic<bool> isRunning;

	const StaticString name;
	const TIndex numHardwareThreads;
	/// @brief The thread that constructed this task system, which is the one expected to drive Engine::Run.
	/// @details Named for what it records. IsBaseThread answers a different question - whether the caller is
	///          running as stream 0 - and the two used to share the word "base".
	const TThreadID engineLoopThreadID;
	TThreadID ioTaskThreadID;
	TStreamArray streams;
	TaskRegistry taskRegistry;

	std::mutex taskQueueMutex;
	BoundedPriorityQueue<WorkItem> taskQueue;

	/// @brief When the budget window last closed, and how many times it has closed. See RunBudgetWindowPass.
	/// @note Written and read only by the base stream's thread, except the count, which is read for diagnosis.
	time::TTime lastBudgetWindowAdvance{};
	std::atomic<std::size_t> numBudgetWindowsAdvanced{0};


public:
	static TIndex GetNumHardwareThreads() noexcept;
	static void SetThreadName(StaticString name) noexcept;
	static void SetStreamIndex(TIndex index) noexcept;
	static StaticString GetCurrentStreamName() noexcept;
	static StaticString GetCurrentThreadName() noexcept;
	static TIndex GetCurrentStreamIndex() noexcept;

	/// @brief Whether the caller is running as stream 0. True only inside a task on that stream's thread.
	/// @return False on the thread driving Engine::Run and on any thread the application created, since neither
	///         has been given a stream - see NonStreamIndex.
	static bool IsBaseThread() noexcept;
	/// @brief Whether the caller is running as the IO stream.
	static bool IsIOThread() noexcept;

	static TIndex GetBaseTaskStreamIndex() noexcept
	{
		return BaseStreamIndex;
	}

	static TIndex GetIOTaskStreamIndex() noexcept
	{
		return IOStreamIndex;
	}

public:
	TaskSystem() noexcept;
	~TaskSystem() noexcept;

	void Initialize() noexcept;
	/// @brief Ask the run to end: close every stream except the base stream, which is closed last.
	/// @details Closing means finishing the work a stream already holds rather than stopping in the middle of it, so
	/// the request
	///          that ends a run must not be the thing that abandons queued items. It must also not close the base
	///          stream, which still owns the end of the run - final statistics, the last log lines, and whatever the
	///          closing streams hand back - and log writing is itself a task on a stream, so an executor closed before
	///          the reporting it must carry out turns an orderly end into a flush timeout.
	void RequestShutDown() noexcept;

	/// @brief Ask every stream except the base stream to close, without ending the run. Idempotent.
	/// @details Called by the teardown as well as by the shutdown request, because a wait for streams that were never
	/// asked to
	///          close is a wait that never ends.
	void RequestOtherStreamsClose() noexcept;

	/// @brief Whether every stream except the base stream has drained its work and left its loop.
	[[nodiscard]] bool AreOtherStreamsClosed() noexcept;
	void JoinAndClear() noexcept;

	// Enqueue a task into the general task queue which is a low-priority queue. The task will be executed after
	// performing all existing special queue for eash task stream.
	void Enqueue(const WorkItem& task) noexcept;

	// Take the top priority task from the general task queue if task stream affinity has been set.
	// It'll add an task-stream affinity once it fails to take the top priority task due to its task-stream affinity
	// to prevent blocking the entire task streams by a task with null-affinity
	// @note A thread that has not been given a stream never takes from here, and is not recorded as having seen
	//       anything: its index is NonStreamIndex, which is out of range for the affinity mask and therefore
	//       dropped. Before that index had a distinct value such a thread was treated as stream 0.
	void Dequeue(std::optional<WorkItem>& outTask) noexcept;

	/// @brief Track a task and return the identity that names it.
	/// @return The identity, or a null TaskID when the registry has no free record - see TaskRegistry::Create. A
	///         task that is not tracked cannot be dispatched: its work items carry an identity no stream can
	///         resolve, so they are dropped.
	[[nodiscard]] TaskID CreateTask(StaticString taskName, TRunnable func, void* userData) noexcept;

	/// @brief The tracked task an identity names, or nullptr. See TaskRegistry::Find.
	[[nodiscard]] Task* FindTask(TaskID id) noexcept;

	/// @brief Stop tracking a task and give its record back to the registry. See TaskRegistry::Release.
	void ReleaseTask(TaskID id) noexcept;

	/// @brief Record which task to dispatch when `task`'s join closes. See TaskRegistry::SetSuccessor.
	void SetSuccessor(TaskID task, TaskID successor) noexcept
	{
		taskRegistry.SetSuccessor(task, successor);
	}

	/// @brief Ask to be told if this task's work is dropped without running.
	/// @param handler `nullptr` clears the notice, which is also the default: a task nobody asked about notifies nobody.
	/// @param userData handed to the handler untouched and never cleared by the engine; its lifetime is the requestor's to
	///        guarantee, and it must outlive every drop this task's work can suffer.
	/// @note Call it before the task is offered. Once work is queued the engine may drop an item before this lands, which
	///       loses the notice rather than racing it, and there is deliberately no lock to close that window: a mutex on
	///       the offer path to protect an opt-in courtesy is the trade this engine has already refused twice.
	/// @note The handler runs on the thread that dropped the work, which may be a worker mid-shift. Do not block in it,
	///       and do not look the task record up from it - that record is being dropped around the call.
	void SetAbandonedNotice(TaskID task, FAbandonedNotice handler, void* userData = nullptr) noexcept
	{
		if (auto* target = FindTask(task); target != nullptr)
		{
			target->abandonedNotice = handler;
			target->abandonedUserData = userData;
		}
	}

	/// @brief The successor recorded for a task, or a null ID. See TaskRegistry::GetSuccessor.
	[[nodiscard]] TaskID GetSuccessor(TaskID task) noexcept
	{
		return taskRegistry.GetSuccessor(task);
	}

	/// @brief Deliver a finished task's outcome: copy its result packet onto the successor recorded for it and queue
	///        that successor on the stream the packet names.
	/// @details This is §6.1's arrow, and the thread that closed the join runs it - whichever worker happened to finish
	///          the last work item, not a thread chosen for the purpose. Nothing accumulates in between: there is no
	///          completion buffer, no per-stream queue of finished tasks, no scan of the registry. A task carries its
	///          own outcome, so the outcome is already in the hands of the thread holding the task, and one enqueue is
	///          the whole delivery. Chains work for free: the successor's own work item closes its join the same way
	///          and dispatches in turn, so A can wake B and B wake C without the engine knowing pipelines exist (R9).
	/// @details __What must be true for a successor to run.__ The finishing task must have recorded a successor, and
	/// its
	///          result packet must name a destination stream - a successor with no stream named is a routing decision
	///          nobody has made, so it is reported and left undone rather than guessed at. The successor must still be
	///          tracked: one released while its producer ran is a caller abandoning work it asked to be told about, and
	///          that is reported too. The successor must have reserved at least one subtask, because this queues
	///          exactly one work item covering its whole range. Whatever the successor held in its own packet is
	///          overwritten by the outcome - the successor's packet is the outcome slot, and parameters for a successor
	///          have to travel somewhere the successor reads them.
	/// @details __What one delivery touches.__ A lookup, a 128-byte packet copy, then the destination stream's queue
	///          lock for the length of a push. That lock is this shape's only coupling: a worker delivering into a
	///          stream can wait for that stream's lock, and a stream holds its lock across the lane rotation that picks
	///          its next work item. The alternative - a completion list the base stream drains - moves that wait off
	///          the worker and onto memory, at the price of a container that outlives the task it describes, which is
	///          the shape this design has declined three times: the result buffer at R23, the result container at
	///          `ac496e6`, and the completion list dropped while writing R30. The wait here is one push long and only
	///          ever for a stream somebody named out loud.
	/// @note __Every refusal is reported.__ A successor that is never dispatched stops a chain in one place, and the
	///       symptom is a task that simply never runs - which surfaces as a hang far downstream. Each path that
	///       declines to dispatch names the pair that caused it.
	/// @param finishedTask The identity of the task whose join just closed. A null identity - a task built by hand
	///        rather than created through the task system - has nowhere to have recorded a successor, so nothing runs.
	/// @threadsafe Called by the thread that closed the join, from inside the work item that closed it. It queues the
	///             successor rather than running it, so a chain of any depth costs one push per link and never grows
	///             the caller's stack.
	void DispatchSuccessor(TaskID finishedTask) noexcept;

	/// @brief Split a heavy job into sub-jobs across the streams the caller names, and let the join close on its own.
	/// @details G5 moved splitting out of the task container and into a primitive here, and R10 asked for it as two
	///          functions: one taking the streams to use, one asking for a number of streams and letting the engine
	///          choose them. Neither waits. The engine creates the task, declares the join with ReserveSubTasks (R29),
	///          queues one work item per contiguous non-empty range, round-robin over the chosen streams, and returns.
	///          The last item to report in closes the join, and the join dispatches the successor named by the caller -
	///          so a caller that wants to combine results supplies a task to do the combining, not a thread to park.
	///          Measured against the alternative this retires: a BusyWait occupies a stream thread for the whole
	///          duration, and a stream thread is the scarcest thing the engine has.
	/// @details __Where per-item results go.__ One task holds one 128-byte packet (R23), so the sub-jobs of one split
	///          cannot each own a packet. Per-item output belongs to the caller - memory that `userData` names - and it
	///          has to be written in **indexed slots** rather than accumulated: items reach different streams and
	///          finish in an order nobody controls, so a shared accumulator is a data race that usually passes, which
	///          is worse than one that fails. The combine happens in the successor's runnable, over slots addressed by
	///          index, and must be order-independent or ordered by that index. Deriving a slot from the range start,
	///          which every runnable receives, is how a runnable knows which slot is its own.
	/// @details __What the arguments become.__ Counts of zero or less are refusals, not empty splits: these counts
	///          arrive as the engine's signed stream-index type, and a negative one is not a smaller job - it is a
	///          value that turns into an enormous range the moment it reaches an unsigned index. `numSubJobs` is
	///          clamped down to `numItems`, because an empty range is a work item that reports in for nothing, and
	///          further down to 255 because the join counter is 8 bits.
	///          Both clamps are silent and neither is a refusal: the split still covers every item, with fewer items of
	///          work. A stream index that is not a stream refuses the whole call - a null TaskID comes back and no task
	///          is created - because placing the work is the stated intent of naming streams at all, and silently
	///          dropping part of that list produces a split that runs somewhere else while looking configured. Zero
	///          items or zero sub-jobs is likewise a refusal, since there is nothing that could be a work item.
	/// @param successor Routed before the first work item is queued, never after: a join that closes before the
	///        routing exists delivers to nobody, and with enough streams that is a race rather than a bug report.
	///        Passing a null TaskID means fire-and-forget (R16) - the split runs, its join closes, and nothing is
	///        dispatched, which is the shape for work whose result nobody reads.
	/// @param successorStream Where the successor runs, which a split has to be told and an ordinary task is not. R30
	///        routes an outcome by the destination byte of the producing task's packet, and a split has no producing
	///        task: its packet is written by nobody, because the sub-jobs each own a slice of the work and the combine
	///        happens in the successor. So the byte is filled from this argument, before the first item is queued.
	///        Naming a successor without naming a stream for it refuses the call, which is the same rule R30 applies
	///        to a task that produced a result and named nowhere to send it. A sub-job that later rewrites the packet
	///        overwrites this, and that is how a stage hands its outcome somewhere else on purpose.
	/// @param priority R18's producer-declared band: zero sends every item down the destination stream's FIFO lane,
	///        anything above it sends them down that stream's priority lane. One split uses one band throughout, and
	///        a split cannot carry per-item bands because the items are this call's, not the caller's.
	/// @return The identity of the created task, or a null TaskID if the call was refused.
	/// @note __The caller owns what comes back.__ Release the identity when the packet is no longer wanted: nothing
	///       else will, since the engine has no reason to hold a task whose join has closed, and a split whose
	///       identity was never kept is a record that stays live until the registry is torn down.
	/// @threadsafe Callable from any thread, including from inside a running task's own runnable - a split that
	///             finishes a stage and forks the next one is the shape this is for. It never blocks on a stream: the
	///             only locks taken are the destination streams' queue locks, one push per item.
	[[nodiscard]] TaskID ParallelFor(StaticString taskName, TRunnable func, void* userData, TIndex numItems,
									 TIndex numSubJobs, const TIndex* streamIndices, TIndex numStreamIndices,
									 uint8_t priority = 0, TaskID successor = {},
									 TIndex successorStream = NonStreamIndex) noexcept;

	/// @brief The R10 variant that asks for a number of streams instead of naming them.
	/// @details Streams are chosen from the worker range only - never the base stream and never the IO stream - round
	///          robin from its first stream, which makes the choice reproducible for a given engine configuration.
	///          Asking for more streams than exist clamps to what exists rather than refusing; a caller that needs
	///          specific streams should name them, which is what the other form is for and why both exist (R10: the
	///          explicit form keeps "not the IO stream" expressible, and the same call behaving differently frame to
	///          frame is exactly what makes a regression hard to reproduce).
	/// @return As the explicit form: the created task's identity, or null if the call was refused.
	[[nodiscard]] TaskID ParallelFor(StaticString taskName, TRunnable func, void* userData, TIndex numItems,
									 TIndex numSubJobs, TIndex numStreams, uint8_t priority = 0, TaskID successor = {},
									 TIndex successorStream = NonStreamIndex) noexcept;

	/// @brief The registry itself, for capacity and growth. See TaskRegistry.
	[[nodiscard]] TaskRegistry& GetRegistry() noexcept
	{
		return taskRegistry;
	}

	/// @brief Close every stream's CPU-budget window, to be reopened by each stream on its own thread.
	/// @details An allowance that closes nothing is a measurement that changes no decision, which is a syscall
	///          billed per task for nothing; the window is what makes an allowance mean "per frame period", the
	///          yardstick ConfigureBudget already states allowances against. Cheap enough to call from a loop -
	///          it advances only once a base frame period has passed, and the base stream period is the number
	///          every budget in the engine is measured against, so the two cannot drift apart.
	/// @details Nothing here writes a budget. Each stream is signalled and reopens itself, because a budget
	///          charges the CPU time of the thread that owns it, and a reset executed elsewhere races that
	///          thread's BeginTask and EndTask pairing on the fields that decide whether a charge is a task or a
	///          thread's whole life.
	/// @note The base stream's own thread calls this from its loop, and the timing state it holds is unsynchronised
	///       on purpose: a second caller would need a lock to decide a timestamp.
	/// @brief Run one engine frame of base-stream work inside the base frame budget.
	void Update() noexcept;

	/// @brief Wait for `isDone` by driving the base stream, the only way such a wait can end on this thread.
	template <class Predicate>
	bool DriveUntil(const char* waitingFor, Predicate&& isDone,
					std::chrono::milliseconds patience = std::chrono::milliseconds(30000)) noexcept
	{
		TaskStream& baseStream = GetStream(GetBaseTaskStreamIndex());
		auto remaining = patience;

		if (std::this_thread::get_id() != baseStream.GetThreadID())
		{
			while (!isDone() && remaining > std::chrono::milliseconds::zero())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				remaining -= std::chrono::milliseconds(1);
			}

			return isDone();
		}

		baseStream.SetNestedPumpAllowed(true);

		struct NestedPumpGuard
		{
			TaskStream& stream;

			~NestedPumpGuard()
			{
				stream.SetNestedPumpAllowed(false);
			}
		} nestedPumpGuard{baseStream};

		while (!isDone() && remaining > std::chrono::milliseconds::zero())
		{
			// Update, not baseStream.Update: a callback posted to the engine loop lives on the stream the loop drives,
			// and a stream's pump is what runs it.
			Update();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			remaining -= std::chrono::milliseconds(1);
		}

		if (!isDone())
		{
			// Say it out loud. A caller that proceeds after a failed wait usually blocks on the very thing it was
			// waiting for, and a suite that hangs reports nothing - which is how a missing drain looked: no failing
			// test, only a wall clock.
			ReportDriveTimeout(waitingFor, patience);
		}

		return isDone();
	}

	/// @brief Report that a drive-until wait gave up, naming what it was waiting for and how long it tried.
	/// @details Separate from the template so the reporting does not drag the engine header into every translation unit
	/// that
	///          drives a wait.
	void ReportDriveTimeout(const char* waitingFor, std::chrono::milliseconds patience) noexcept;

	void RunBudgetWindowPass() noexcept;

	/// @brief How many budget windows have closed since the task system was built.
	/// @details The witness that the pass is alive. A stream that reopens looks exactly like a stream that was
	///          never throttled, so the pass needs a number of its own to be provable.
	[[nodiscard]] std::size_t GetNumBudgetWindowsAdvanced() const noexcept
	{
		return numBudgetWindowsAdvanced.load(std::memory_order_relaxed);
	}

	/// @brief Queue a task on one stream by index.
	/// @details Cannot refuse. A stream used to refuse a task that declared more results than it could ever fit,
	///          and that question stopped existing when a result moved into the task's own record - the record is
	///          allocated when the task is created, so there is no capacity left for a stream to run short of.
	/// @note An index outside the streams is a programming error and asserts; the general-queue overload above
	///       needs no index, and takes the task without choosing a stream.
	/// @brief Queue one work item on one stream, on the lane the caller names.
	/// @param lane `StreamDrainPolicy::ELane::Fifo` runs items in arrival order; `Priority` runs the highest `priority`
	///        first, oldest within a tie. `ELane::None` is a caller error and is asserted, because a provider attached to
	///        no lane is a different condition from work with nowhere to go.
	/// @note **Lane and priority are two different things and this API keeps them apart.** The lane is which queue serves
	///       the work; `WorkItem::priority` is the ordering *inside* the priority queue. Before this overload existed the
	///       lane was decided by which internal function a caller happened to reach, and the engine's public surface could
	///       only reach the FIFO lane - so the priority lane, its share of the drain rate, and its half of the shutdown
	///       drain were unreachable from the public API. See decision 5 in `.Plans/TODO_task_system.md`.
	void Enqueue(TIndex streamIndex, const WorkItem& task, StreamDrainPolicy::ELane lane) noexcept;

	/// @brief Queue one work item on the FIFO lane of one stream. Equivalent to `Enqueue(streamIndex, task, ELane::Fifo)`.
	void Enqueue(TIndex streamIndex, const WorkItem& task) noexcept;

	/// @brief Queue a whole task on one stream, which is the customer's entry point for single-shot work.
	/// @details The task declares its own join and the engine builds the item that fills it, which is the only order in
	/// which
	///          those two facts can be stated without the caller having to know anything about items. A customer that
	///          builds an item itself has reached inside the engine for a handle it should not hold, and this is the
	///          call it wants instead.
	/// @param streamIndex Which stream takes the work. An index outside the streams asserts, as with Enqueue.
	/// @param task The task to run. It must have been issued by the registry and must not already declare a join - this
	/// declares
	///             one of exactly one item, and calling it twice on the same task would overwrite the first
	///             declaration.
	/// @param Priority of the item, 0 being the least urgent.
	/// @brief Offer a task to one stream as a single whole-range work item, on the lane the caller names.
	/// @param priority ordering key inside the priority queue - it does **not** choose a lane; see `lane`.
	/// @param lane which queue serves the work, `ELane::Fifo` by default so existing callers keep today's behaviour.
	/// @note The lane parameter is added last and defaulted for one reason: a `uint8_t` priority and a scoped lane enum
	///       next to each other invite a call like `EnqueueTask(i, t, 1)` binding to the lane instead of the priority.
	void EnqueueTask(TIndex streamIndex, Task& task, uint8_t priority = 0,
					 StreamDrainPolicy::ELane lane = StreamDrainPolicy::ELane::Fifo) noexcept;

	// Dispatch a task to be executed on the main thread.
	// The task will be queued and executed when the main thread processes its queue.
	// priority: 0 = least urgent, 255 = most urgent. The default is 128 to match MainThreadTaskQueue's own
	// default; it used to be 0, which under the old "0 = highest" convention meant every caller of this
	// function was silently enqueuing at top priority. With the direction inverted, leaving 0 here would
	// have flipped those same callers to the bottom of the queue instead.
	void DispatchToMainThread(TMainThreadTask task, void* userData, uint8_t priority = 128) noexcept;

	// Process all pending main thread tasks.

	[[nodiscard]] StaticString GetName() const noexcept
	{
		return name;
	}

	[[nodiscard]] auto& IsRunning() const noexcept
	{
		return isRunning;
	}

	auto& GetBaseTaskStream() noexcept
	{
		return streams[GetBaseTaskStreamIndex()];
	}

	auto& GetBaseTaskStream() const noexcept
	{
		return streams[GetBaseTaskStreamIndex()];
	}


	auto& GetIOTaskStream() noexcept
	{
		return streams[GetIOTaskStreamIndex()];
	}

	auto& GetIOTaskStream() const noexcept
	{
		return streams[GetIOTaskStreamIndex()];
	}

	[[nodiscard]] StaticString GetStreamName(int index) const noexcept;
	[[nodiscard]] TIndex GetStreamIndex(TThreadID id) const noexcept;
	TaskStream& GetStream(int index) noexcept;

	/// @brief Whether the stream at `index` exists at this instant.
	/// @details Streams are built by `Initialize` and cleared by `JoinAndClear`, so the array is empty
	///          before startup and again once the pump has joined; indexing either way aborts with a
	///          bare `FatalAssert`. Anything that reaches a stream by index instead of holding one must
	///          ask first - this is the precondition of `GetStream` and `GetIOTaskStream`, not a policy.
	/// @note Answers "can I index this now", nothing more. It is not a lifetime guarantee across the
	///       call: a concurrent `JoinAndClear` can retire the stream between the test and the use, so a
	///       caller logging while another thread tears the pump down still needs its own ordering.
	/// @brief Cap on how many streams one split may spread across, so the chosen list can live on the caller's stack.
	/// @note Not a tuning knob: a job spread over more than this many streams is not a shape the engine has, and the
	///       clamp keeps the choice reproducible instead of allocating.
	static constexpr TIndex MaxStreamsPerSplit = 64;

	TaskID RunSplit(StaticString taskName, TRunnable func, void* userData, TIndex numItems, TIndex numSubJobs,
					const TIndex* streamIndices, TIndex numStreamIndices, uint8_t priority, TaskID successor,
					TIndex successorStream) noexcept;

	[[nodiscard]] bool HasStream(TIndex index) const noexcept
	{
		return streams.IsValidIndex(index);
	}

private:
	void BuildStreams();
};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

class TaskSystemTest : public TestCollection
{
public:
	TaskSystemTest()
		: TestCollection("TaskSystemTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
