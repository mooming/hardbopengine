// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


#include <atomic>
#include <thread>

#include "Container/Array.h"
#include "Container/BoundedPriorityQueue.h"
#include "MainThreadTaskQueue.h"
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
	BoundedPriorityQueue<RangedTask> taskQueue;

	/// @brief When the budget window last closed, and how many times it has closed. See RunBudgetWindowPass.
	/// @note Written and read only by the base stream's thread, except the count, which is read for diagnosis.
	time::TTime lastBudgetWindowAdvance{};
	std::atomic<std::size_t> numBudgetWindowsAdvanced{0};

	MainThreadTaskQueue mainThreadTaskQueue;

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
	void RequestShutDown() noexcept;
	void JoinAndClear() noexcept;

	// Enqueue a task into the general task queue which is a low-priority queue. The task will be executed after
	// performing all existing special queue for eash task stream.
	void Enqueue(const RangedTask& task) noexcept;

	// Take the top priority task from the general task queue if task stream affinity has been set.
	// It'll add an task-stream affinity once it fails to take the top priority task due to its task-stream affinity
	// to prevent blocking the entire task streams by a task with null-affinity
	// @note A thread that has not been given a stream never takes from here, and is not recorded as having seen
	//       anything: its index is NonStreamIndex, which is out of range for the affinity mask and therefore
	//       dropped. Before that index had a distinct value such a thread was treated as stream 0.
	void Dequeue(std::optional<RangedTask>& outTask) noexcept;

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

	/// @brief The successor recorded for a task, or a null ID. See TaskRegistry::GetSuccessor.
	[[nodiscard]] TaskID GetSuccessor(TaskID task) noexcept
	{
		return taskRegistry.GetSuccessor(task);
	}

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
	void Enqueue(TIndex streamIndex, const RangedTask& task) noexcept;

	// Dispatch a task to be executed on the main thread.
	// The task will be queued and executed when the main thread processes its queue.
	// priority: 0 = least urgent, 255 = most urgent. The default is 128 to match MainThreadTaskQueue's own
	// default; it used to be 0, which under the old "0 = highest" convention meant every caller of this
	// function was silently enqueuing at top priority. With the direction inverted, leaving 0 here would
	// have flipped those same callers to the bottom of the queue instead.
	void DispatchToMainThread(TMainThreadTask task, void* userData, uint8_t priority = 128) noexcept;

	// Process all pending main thread tasks.
	size_t ProcessMainThreadTasks() noexcept;

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

	auto& GetMainThreadTaskQueue() noexcept
	{
		return mainThreadTaskQueue;
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
