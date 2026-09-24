// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TaskSystem.h"

#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <limits>
#include <thread>

#include "Config/ConfigParam.h"
#include "Constants.h"
#include "Log/Logger.h"

namespace hbe
{

namespace
{
thread_local StaticString ThreadName;
thread_local TaskSystem::TIndex StreamIndex = TaskSystem::NonStreamIndex;
} // namespace

TaskSystem::TIndex TaskSystem::GetNumHardwareThreads() noexcept
{
	const auto hardwareConcurrency = std::thread::hardware_concurrency();
	const auto numAvaibleHardwareThreads = static_cast<TIndex>(hardwareConcurrency);

	return numAvaibleHardwareThreads;
}

void TaskSystem::SetThreadName(StaticString name) noexcept
{
	ThreadName = name;
}

void TaskSystem::SetStreamIndex(TIndex index) noexcept
{
	StreamIndex = index;
}

StaticString TaskSystem::GetCurrentStreamName() noexcept
{
	return ThreadName;
}

StaticString TaskSystem::GetCurrentThreadName() noexcept
{
	return ThreadName;
}

TaskSystem::TIndex TaskSystem::GetCurrentStreamIndex() noexcept
{
	return StreamIndex;
}

bool TaskSystem::IsBaseThread() noexcept
{
	return StreamIndex == BaseStreamIndex;
}

bool TaskSystem::IsIOThread() noexcept
{
	return StreamIndex == IOStreamIndex;
}

TaskSystem::TaskSystem() noexcept
	: isRunning(false)
	, name("TaskSystem")
	, numHardwareThreads(GetNumHardwareThreads())
	, engineLoopThreadID(std::this_thread::get_id())
{
	FatalAssert(numHardwareThreads > 0, "It should have at least one hardware thread.");
}

TaskSystem::~TaskSystem() noexcept
{
	JoinAndClear();
}

void TaskSystem::Initialize() noexcept
{
	auto& logger = Logger::Get();
	auto logFilter = [](auto level)
	{
		static TAtomicConfigParam<uint8_t> logLevel("Log.TaskSystem", "The TaskSystem Log Level",
													static_cast<uint8_t>(ELogLevel::Warning));

		return level > static_cast<ELogLevel>(logLevel.Get());
	};

	logger.SetFilter(GetName(), logFilter);

	auto log = Logger::Get(GetName());
	log.Out([this](auto& ls) { ls << "Hardware Concurrency = " << numHardwareThreads; });

	static TAtomicConfigParam<std::size_t> initialRecords(
			"Task.RegistryInitialRecords", "Task records the registry starts with, in banks of the grow-by figure",
			TaskRegistry::DefaultInitialCapacityRecords);
	static TAtomicConfigParam<std::size_t> growByRecords("Task.RegistryGrowByRecords",
														 "Task records each growth of the task record table",
														 TaskRegistry::DefaultGrowByRecords);
	static TAtomicConfigParam<std::size_t> maxRecords("Task.RegistryMaxRecords",
													  "Largest the task record table may grow. 0 means no ceiling",
													  TaskRegistry::DefaultMaxCapacityRecords);

	taskRegistry.Initialize("TaskRegistry", initialRecords.Get(), growByRecords.Get());
	taskRegistry.SetMaxCapacity(maxRecords.Get());

	BuildStreams();
}

void TaskSystem::RequestShutDown() noexcept
{
	isRunning = false;

	RequestOtherStreamsClose();
}

void TaskSystem::RequestOtherStreamsClose() noexcept
{
	// No streams means nothing to ask. Reaching for index 0 of an empty array is an index assert, and this path is
	// reached a second time from the destructor after the streams have already been cleared.
	if (!HasStream(GetBaseTaskStreamIndex()))
	{
		return;
	}

	TaskStream& baseStream = GetStream(GetBaseTaskStreamIndex());

	// The IO stream stays open with the base stream, because log writing is a task that runs on it: closing the
	// executor of the reporting before the reporting is done leaves a drain task in flight that can never run again,
	// and every late log line then waits on it.
	TaskStream& ioStream = GetStream(GetIOTaskStreamIndex());

	for (auto& stream : streams)
	{
		if (&stream != &baseStream && &stream != &ioStream)
		{
			stream.RequestClose();
		}
	}
}

bool TaskSystem::AreOtherStreamsClosed() noexcept
{


	const TaskStream& baseStream = GetStream(GetBaseTaskStreamIndex());
	const TaskStream& ioStream = GetStream(GetIOTaskStreamIndex());

	for (const auto& stream : streams)
	{
		if (&stream == &baseStream || &stream == &ioStream)
		{
			continue;
		}

		if (!stream.IsClosed())
		{
			return false;
		}
	}

	return true;
}

void TaskSystem::ReportDriveTimeout(const char* waitingFor, const std::chrono::milliseconds patience) noexcept
{
	Logger::Get().AddLog(GetName(), ELogLevel::Error, [waitingFor, patience](auto& logStream)
	{
		logStream << "A thread driving the engine loop gave up waiting for " << waitingFor << " after "
				  << patience.count()
				  << "ms. The work was still queued, so nothing was going to run it, and a caller that proceeds from "
					 "here "
					 "usually blocks on the very thing it waited for. The intake that should have drained it is the "
					 "first "
					 "thing to look at.";
	});
}

void TaskSystem::Update() noexcept
{
	auto& baseStream = GetStream(GetBaseTaskStreamIndex());
	const auto deadline = std::chrono::steady_clock::now() +
						  std::chrono::duration_cast<std::chrono::nanoseconds>(time::GetBaseFramePeriod());

	while (std::chrono::steady_clock::now() < deadline)
	{
		if (!baseStream.Update())
		{
			break;
		}
	}
}

void TaskSystem::JoinAndClear() noexcept
{
	const bool isEngineLoopThread = std::this_thread::get_id() == engineLoopThreadID;

	// Teardown can be reached without the ordered shutdown, since the destructor is a caller. Asking is idempotent, and
	// not asking would make the wait below one that never ends.
	const bool hasStreams = HasStream(GetBaseTaskStreamIndex());

	if (hasStreams)
	{
		RequestOtherStreamsClose();
	}

	if (isEngineLoopThread && hasStreams)
	{
		TaskStream& baseStream = GetStream(GetBaseTaskStreamIndex());

		while (!AreOtherStreamsClosed() || baseStream.HasPostedTasks() || baseStream.CountPendingItems() > 0)
		{
			baseStream.Update();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		// Log lines are written by a task running on a stream, so the flush that ends the run has to happen while that
		// stream is still running its own work. Joining first leaves the flush waiting on an executor that has already
		// exited, which is a timeout and a fatal assert rather than a shutdown.
		Logger::Get().Flush();

		// The driver thread outlives the task system, so the stream must be taken back before the streams are freed.
		// After this the logger writes its queue directly, which keeps it working to the very last line of the process.
		Logger::Get().SetIODriver(nullptr);
		GetIOTaskStream().CloseDrivenStream();
		baseStream.CloseDrivenStream();

		// The base stream rides the engine loop as well, so it too never reaches RunLoop and must be closed from
		// outside. Its close comes after the IO stream's because it is the last executor left, and the assert below
		// refuses to destroy a stream that was never closed - which is what caught this.

		// The IO stream has no thread to join and no loop that could report its own ending, so the shutdown closes it
		// here, after the driver is withdrawn and cannot be inside a pass. Both engine streams then end inside the
		// engine's shutdown instead of one of them ending silently whenever the logger is destroyed. The logger itself
		// is unaffected: its thread exists before this stream does and keeps writing the log queue directly after this
		// stream is gone.

		// The reporting the shutdown exists to produce is finished, so the streams that carried it close last, and in
		// this order: the executor of log work first, then the base stream that drove the end of the run.
		GetStream(GetIOTaskStreamIndex()).RequestClose();
	}

	if (hasStreams)
	{
		GetStream(GetBaseTaskStreamIndex()).RequestClose();
	}

	for (auto& stream : streams)
	{
		auto& thread = stream.GetThread();
		if (unlikely(!thread.joinable()))
		{
			continue;
		}

		thread.join();
	}

	// Nothing is destroyed that was not closed first. A stream left unclosed here means either a new stream kind that
	// nobody asked to close, or a thread that is still running while its queues are being freed - and both are far
	// cheaper to find as a named assert than as a crash in a later run.
	for (const auto& stream : streams)
	{
		Assert(stream.IsClosed(), "Stream ", stream.GetName(),
			   " is being destroyed without having been closed. Every stream must be closed - drained if it owns a "
			   "thread, "
			   "marked closed by its driver if it rides one - before the task system tears itself down.");
	}

	streams.Clear();
}

void TaskSystem::Enqueue(const WorkItem& task) noexcept
{
	std::scoped_lock<std::mutex> lock(taskQueueMutex);
	taskQueue.Push(task);
}

void TaskSystem::Dequeue(std::optional<WorkItem>& outTask) noexcept
{
	std::scoped_lock<std::mutex> lock(taskQueueMutex);
	if (taskQueue.IsEmpty())
	{
		outTask.reset();
		return;
	}

	auto workItemOpt = taskQueue.Top();
	if (!workItemOpt.has_value())
	{
		outTask.reset();
		return;
	}

	const WorkItem& workItem = workItemOpt.value();
	const unsigned int streamIndex = GetCurrentStreamIndex();

	auto& affinity = workItem.affinity;
	if (!affinity.Get(streamIndex))
	{
		affinity.Set(streamIndex);
		return;
	}

	outTask = workItem;
	(void) taskQueue.Pop();
}

TaskID TaskSystem::CreateTask(StaticString taskName, TRunnable func, void* userData) noexcept
{
	return taskRegistry.Create(taskName, func, userData);
}

Task* TaskSystem::FindTask(TaskID id) noexcept
{
	return taskRegistry.Find(id);
}

void TaskSystem::ReleaseTask(TaskID id) noexcept
{
	taskRegistry.Release(id);
}

void TaskSystem::EnqueueTask(const TIndex streamIndex, Task& task, const uint8_t priority) noexcept
{
	task.ReserveSubTasks(1);
	Enqueue(streamIndex, task.GenerateSubTask(0, 1, priority));
}

void TaskSystem::Enqueue(const TIndex streamIndex, const WorkItem& task) noexcept
{
	if (!streams.IsValidIndex(streamIndex))
	{
		Assert(false, "Invalid stream index %d", streamIndex);
		return;
	}

	streams[streamIndex].EnqueueFifo(task);
}

void TaskSystem::RunBudgetWindowPass() noexcept
{
	const auto now = time::GetNow();
	const auto period = time::GetBaseFramePeriod();

	if (lastBudgetWindowAdvance != time::TTime{} && now - lastBudgetWindowAdvance < period)
	{
		return;
	}

	lastBudgetWindowAdvance = now;
	numBudgetWindowsAdvanced.fetch_add(1, std::memory_order_relaxed);

	for (auto& stream : streams)
	{
		stream.RequestWindowAdvance();
	}
}

void TaskSystem::DispatchSuccessor(TaskID finishedTask) noexcept
{
	Task* finished = taskRegistry.Find(finishedTask);
	if (finished == nullptr)
	{
		return;
	}

	const TaskID successorID = taskRegistry.GetSuccessor(finishedTask);
	if (successorID.IsNull())
	{
		return;
	}

	const ResultPacket& packet = finished->GetResult();
	const std::uint8_t destination = packet.GetDestinationStreamIndex();
	if (destination == ResultPacket::NoDestinationStream)
	{
		auto log = Logger::Get(GetName());
		log.OutError([finishedTask, successorID](auto& ls)
		{
			ls << "Task record " << finishedTask.index << " generation " << finishedTask.generation
			   << " recorded a successor and closed its join, but its result packet names no destination stream, so"
			   << " record " << successorID.index << " generation " << successorID.generation
			   << " was never dispatched. A chain that stops here has no symptom until something downstream waits. ";
		});
		return;
	}

	if (!HasStream(destination))
	{
		auto log = Logger::Get(GetName());
		const auto numStreams = streams.Size();
		log.OutError([finishedTask, successorID, destination, numStreams](auto& ls)
		{
			ls << "Task record " << finishedTask.index << " generation " << finishedTask.generation
			   << " addressed stream " << static_cast<unsigned int>(destination) << " for record " << successorID.index
			   << " generation " << successorID.generation << ", and this engine has " << numStreams
			   << " streams. Nothing was dispatched. ";
		});
		return;
	}

	Task* successor = taskRegistry.Find(successorID);
	if (successor == nullptr)
	{
		auto log = Logger::Get(GetName());
		log.OutError([finishedTask, successorID](auto& ls)
		{
			ls << "Task record " << finishedTask.index << " generation " << finishedTask.generation
			   << " finished, and its successor, record " << successorID.index << " generation "
			   << successorID.generation << ", is no longer tracked. A successor released while its producer ran"
			   << " abandoned the outcome it asked for, so nothing was dispatched. ";
		});
		return;
	}

	if (successor->NumSubTasks() == 0)
	{
		auto log = Logger::Get(GetName());
		log.OutError([successorID](auto& ls)
		{
			ls << "Record " << successorID.index << " generation " << successorID.generation
			   << " is named as a successor but reserved no subtask, so there is no work item to queue for it: an"
			   << " outcome is delivered as one item covering the successor's whole range, which is one reserved"
			   << " subtask. Nothing was dispatched. ";
		});
		return;
	}

	successor->GetResult() = packet;

	streams[destination].EnqueueFifo(successor->GenerateSubTask(0, 1));
}

TaskID TaskSystem::ParallelFor(StaticString taskName, TRunnable func, void* userData, TIndex numItems,
							   TIndex numSubJobs, const TIndex* streamIndices, TIndex numStreamIndices,
							   uint8_t priority, TaskID successor, TIndex successorStream) noexcept
{
	if (streamIndices == nullptr || numStreamIndices == 0)
	{
		auto log = Logger::Get(GetName());
		log.OutError([taskName](auto& ls)
		{
			ls << "ParallelFor of task " << taskName.c_str() << " named no stream to spread across, so nothing was"
			   << " split. Naming the streams is the point of this form. ";
		});
		return {};
	}

	for (TIndex candidate = 0; candidate < numStreamIndices; ++candidate)
	{
		if (!HasStream(streamIndices[candidate]))
		{
			auto log = Logger::Get(GetName());
			const auto numStreams = streams.Size();
			log.OutError([taskName, candidate, numStreams](auto& ls)
			{
				ls << "ParallelFor of task " << taskName.c_str() << " named stream " << candidate << " of "
				   << numStreams << ", which this engine does not have, so the split was refused rather than run on"
				   << " the streams that happened to be left. ";
			});
			return {};
		}
	}

	return RunSplit(taskName, func, userData, numItems, numSubJobs, streamIndices, numStreamIndices, priority,
					successor, successorStream);
}

TaskID TaskSystem::ParallelFor(StaticString taskName, TRunnable func, void* userData, TIndex numItems,
							   TIndex numSubJobs, TIndex numStreams, uint8_t priority, TaskID successor,
							   TIndex successorStream) noexcept
{
	if (numStreams <= 0)
	{
		auto log = Logger::Get(GetName());
		log.OutError([taskName, numStreams](auto& ls)
		{
			ls << "ParallelFor of task " << taskName.c_str() << " asked for " << numStreams
			   << " streams, which is nothing to spread across, so nothing was split. ";
		});
		return {};
	}

	const TIndex firstWorker = GetIOTaskStreamIndex() + 1;
	const TIndex numWorkers = streams.Size() > firstWorker ? streams.Size() - firstWorker : 0;

	if (numWorkers == 0)
	{
		auto log = Logger::Get(GetName());
		log.OutError([taskName, numStreams = numStreams](auto& ls)
		{
			ls << "ParallelFor of task " << taskName.c_str() << " asked for " << numStreams
			   << " streams and this engine has no worker stream: every stream it has is the base stream or the IO"
			   << " stream, and a split is never placed on either of its own accord. ";
		});
		return {};
	}

	TIndex chosen[MaxStreamsPerSplit];
	const TIndex usable = std::min({numStreams, numWorkers, MaxStreamsPerSplit});
	for (TIndex index = 0; index < usable; ++index)
	{
		chosen[index] = firstWorker + index;
	}

	return RunSplit(taskName, func, userData, numItems, numSubJobs, chosen, usable, priority, successor,
					successorStream);
}

TaskID TaskSystem::RunSplit(StaticString taskName, TRunnable func, void* userData, TIndex numItems, TIndex numSubJobs,
							const TIndex* streamIndices, TIndex numStreamIndices, uint8_t priority, TaskID successor,
							TIndex successorStream) noexcept
{
	if (numItems <= 0 || numSubJobs <= 0)
	{
		auto log = Logger::Get(GetName());
		log.OutError([taskName, numItems, numSubJobs](auto& ls)
		{
			ls << "ParallelFor of task " << taskName.c_str() << " over " << numItems << " items in " << numSubJobs
			   << " sub-jobs has nothing to queue. Counts of zero or less are refusals, not empty splits: these counts "
				  "are signed, and a negative one becomes an enormous range the moment it is used as an index. ";
		});
		return {};
	}

	if (!successor.IsNull() && !HasStream(successorStream))
	{
		auto log = Logger::Get(GetName());
		log.OutError([taskName, successorStream](auto& ls)
		{
			ls << "ParallelFor of task " << taskName.c_str() << " named a successor and named stream "
			   << successorStream
			   << " for it, which is not a stream this engine has, so the split was refused. A split has no producer "
				  "to write the routing into its packet, so this call is where that byte comes from, and a byte left "
				  "unwritten is an outcome with nowhere to go. ";
		});
		return {};
	}

	if (numSubJobs > numItems)
	{
		numSubJobs = numItems;
	}

	if (numSubJobs > Task::MaxNumSubTasks)
	{
		numSubJobs = Task::MaxNumSubTasks;
	}

	const TaskID taskID = CreateTask(taskName, func, userData);
	Task* task = FindTask(taskID);
	if (task == nullptr)
	{
		auto log = Logger::Get(GetName());
		log.OutError([taskName](auto& ls)
		{
			ls << "ParallelFor could not get a record for task " << taskName.c_str()
			   << ", so the split was not queued. The registry is at its capacity. ";
		});
		return {};
	}

	if (!successor.IsNull())
	{
		SetSuccessor(taskID, successor);
		task->GetResult().SetDestinationStreamIndex(static_cast<std::uint8_t>(successorStream));
	}

	task->ReserveSubTasks(static_cast<Task::TNumSubTasks>(numSubJobs));

	TIndex itemStart = 0;
	TIndex destinationSlot = 0;

	for (TIndex subJob = 0; subJob < numSubJobs; ++subJob)
	{
		const TIndex itemsLeft = numItems - itemStart;
		const TIndex jobsLeft = numSubJobs - subJob;
		const TIndex itemEnd = itemStart + (itemsLeft + jobsLeft - 1) / jobsLeft;

		auto& destination = streams[streamIndices[destinationSlot]];
		const auto item = task->GenerateSubTask(itemStart, itemEnd, priority);
		if (priority > 0)
		{
			destination.EnqueuePriority(item);
		}
		else
		{
			destination.EnqueueFifo(item);
		}

		itemStart = itemEnd;
		destinationSlot = (destinationSlot + 1) % numStreamIndices;
	}

	return taskID;
}

void TaskSystem::DispatchToMainThread(TMainThreadTask taskFunc, void* userData, uint8_t priority) noexcept
{
	GetStream(GetBaseTaskStreamIndex()).DispatchPostedTasks(taskFunc, userData, priority);
}

StaticString TaskSystem::GetStreamName(int index) const noexcept
{
	if (unlikely(!streams.IsValidIndex(index)))
	{
		static StaticString unknown("Unknown");
		return unknown;
	}

	return streams[index].GetName();
}

TaskSystem::TIndex TaskSystem::GetStreamIndex(TThreadID id) const noexcept
{
	TIndex index = NonStreamIndex;

	auto size = streams.Size();
	for (decltype(size) i = 0; i < size; ++i)
	{
		if (auto& stream = streams[i]; stream.GetThreadID() != id)
		{
			continue;
		}

		index = i;
		break;
	}

	return index;
}

TaskStream& TaskSystem::GetStream(int index) noexcept
{
	if (index < 0 || index >= streams.Size())
	{
		return streams[0];
	}

	return streams[index];
}

void TaskSystem::BuildStreams()
{
	// The old check here was IsBaseThread, which every thread passed until it was given a stream - it could not
	// have failed. What this function actually requires is to run on the thread that will drive the engine.
	Assert(std::this_thread::get_id() == engineLoopThreadID);
	FatalAssert(numHardwareThreads >= ENGINE_MIN_HARDWARE_THREADS,
				"Number of hardware threads are less than the minimum requirement");

	SetThreadName(TaskSystem::EngineLoopThreadName);

	TIndex workerIndexStart = 0;

	auto log = Logger::Get(GetName());
	log.Out("# Creating TaskStreams ======================");

	streams.Swap(Array<TaskStream>(numHardwareThreads));

	// Pre-defined Engine Task Streams
	{
		auto index = GetBaseTaskStreamIndex();
		streams.Emplace(index, "Base", index);

		index = GetIOTaskStreamIndex();
		streams.Emplace(index, "IO", index);
	}

	workerIndexStart = GetIOTaskStreamIndex() + 1;

	TIndex numWorkers = 0;

	InlineStringBuilder<64> streamName;
	for (TIndex i = workerIndexStart; i < numHardwareThreads; ++i)
	{
		++numWorkers;
		streamName << "Worker" << numWorkers;

		streams.Emplace(i, streamName.c_str(), i);
		streamName.Clear();
	}

	log.Out("# Starting TaskStreams ======================");

	isRunning = true;

	for (auto& stream : streams)
	{
		stream.Start(*this);
	}

	// The IO stream has no thread to claim, so its driver is named here: the logger's own thread drives it from now on.
	Logger::Get().SetIODriver(&GetIOTaskStream());
}
} // namespace hbe

#ifdef __UNIT_TEST__
#include <memory>
#include "../Engine/Engine.h"
#include "OSAL/Intrinsic.h"
#include "Test/TestCollection.h"

namespace hbe
{

namespace
{

/// @brief Advance the budget window from this thread until a predicate holds, and say whether it came to hold.
/// @details A stream that has spent its allowance takes nothing from its own lanes - StreamDrainPolicy::ChooseLane
///          returns None for an exhausted round - so a task queued to such a stream runs only once a window reopens
///          it. Windows are advanced by the base stream's own thread, which is the thread a test occupies by
///          running, so a test that merely waited was waiting for itself to be scheduled. Measured as a hang for the
///          life of the process before it was written this way.
/// @note A window advance also zeroes the stream's accumulated CPU, so a predicate that reads that figure is
///       reading something this helper destroys. Every predicate used with it counts things a reopen cannot erase:
///       tasks that ran, and refusals recorded.
template <typename TPredicate>
bool AdvanceWindowsUntil(TaskSystem& taskSys, const TPredicate& holds, std::chrono::milliseconds patience) noexcept
{
	const auto deadline = std::chrono::steady_clock::now() + patience;
	while (!holds() && std::chrono::steady_clock::now() < deadline)
	{
		taskSys.RunBudgetWindowPass();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return holds();
}

class TrackedTask final
{
public:
	TaskSystem& taskSystem;
	TaskID id;
	Task* task;

	TrackedTask(StaticString taskName, TRunnable func, void* userData) noexcept
		: taskSystem(Engine::Get().GetTaskSystem())
		, id(taskSystem.CreateTask(taskName, func, userData))
		, task(taskSystem.FindTask(id))
	{
		FatalAssert(task != nullptr, "The task registry could not track a test task, so that test cannot run.");
	}

	TrackedTask(const TrackedTask& other) = delete;
	TrackedTask& operator=(const TrackedTask& other) = delete;

	~TrackedTask() noexcept
	{
		taskSystem.ReleaseTask(id);
	}

	[[nodiscard]] Task& operator*() const noexcept
	{
		return *task;
	}
};

/// @brief The identity, the routing and the counters of one task in an outcome-delivery test.
/// @details One fixture per task rather than one per test, because a runnable receives user data and an index range
///          and nothing else: under R23 a task writes its outcome into its own packet, which it reaches by its own
///          identity through the registry, so the writer has to know who it is. Sharing one fixture between a producer
///          and its successor would make `self` mean two things at once.
/// @note `destination` left at NoDestinationStream is how a test expresses "this task produced a result and never
///       named a stream", which is the half-filled routing case DispatchSuccessor has to refuse out loud.
struct DeliveryFixture final
{
	TaskSystem* taskSystem{nullptr};
	TaskID self{};
	TaskID next{};
	std::uint8_t destination{ResultPacket::NoDestinationStream};

	std::atomic<unsigned> stageOneRuns{0};
	std::atomic<unsigned> stageTwoRuns{0};
	std::atomic<unsigned> stageThreeRuns{0};
	std::atomic<unsigned> sentinelRuns{0};
	std::atomic<std::uint8_t> deliveredKind{ResultPacket::KindNoResult};
	std::atomic<std::uint8_t> deliveredPayload{0};
};

/// @brief Write an outcome into the packet of the task named by `writer`, addressed through the registry.
void WriteOutcome(DeliveryFixture& fixture, TaskID writer, std::uint8_t kind, std::uint8_t outcomeByte) noexcept
{
	Task* task = fixture.taskSystem->FindTask(writer);
	if (task == nullptr)
	{
		return;
	}

	ResultPacket& packet = task->GetResult();
	packet.SetKind(kind);
	packet.SetDestinationStreamIndex(fixture.destination);
	packet.GetPayload()[0] = outcomeByte;
}

/// @brief Produce an outcome and stop. The first link of a chain, and a task whose join closes normally.
std::size_t RunStageOne(void* userData, TIndex startIndex, TIndex endIndex) noexcept
{
	DeliveryFixture& fixture = *static_cast<DeliveryFixture*>(userData);
	fixture.stageOneRuns.fetch_add(1, std::memory_order_relaxed);
	WriteOutcome(fixture, fixture.self, ResultPacket::FirstApplicationKind, 0xA5);
	return static_cast<std::size_t>(endIndex - startIndex);
}

/// @brief Record a successor from inside the running task, then produce an outcome for it. The middle link.
/// @details Recording from inside the runnable, rather than by the test before enqueueing, is deliberate: it is the
///          shape a real producer has, where the task to wake is only known once the work has been done. With `next`
///          left null it clears a successor, which the registry accepts and which no chain should follow.
std::size_t RunStageTwo(void* userData, TIndex startIndex, TIndex endIndex) noexcept
{
	DeliveryFixture& fixture = *static_cast<DeliveryFixture*>(userData);
	fixture.stageTwoRuns.fetch_add(1, std::memory_order_relaxed);
	fixture.taskSystem->SetSuccessor(fixture.self, fixture.next);
	WriteOutcome(fixture, fixture.self, ResultPacket::FirstApplicationKind, 0x5A);
	return static_cast<std::size_t>(endIndex - startIndex);
}

/// @brief Read the outcome this task was dispatched with, and count the run. The last link.
std::size_t RunStageThree(void* userData, TIndex startIndex, TIndex endIndex) noexcept
{
	DeliveryFixture& fixture = *static_cast<DeliveryFixture*>(userData);
	fixture.stageThreeRuns.fetch_add(1, std::memory_order_relaxed);

	Task* task = fixture.taskSystem->FindTask(fixture.self);
	if (task != nullptr)
	{
		fixture.deliveredKind.store(task->GetResult().GetKind(), std::memory_order_relaxed);
		fixture.deliveredPayload.store(task->GetResult().GetPayload()[0], std::memory_order_relaxed);
	}

	return static_cast<std::size_t>(endIndex - startIndex);
}

/// @brief Count a run and nothing else.
/// @details This is the in-band barrier the delivery tests are built on, and it exists because a negative cannot be
///          slept for. Queued behind the subject task on the same lane, it can only start once the subject's work item
///          has left the stream - and DispatchSuccessor runs inside that item, before the stream takes anything else -
///          so its run is proof the delivery attempt is over, rather than an amount of time that might be enough.
///          Measured before this existed: a test that slept 200 ms and then looked found what the machine happened to
///          have done in 200 ms, which passed on a loaded machine for the wrong reason and failed on an idle one for
///          the right one.
std::size_t RunSentinel(void* userData, TIndex startIndex, TIndex endIndex) noexcept
{
	DeliveryFixture& fixture = *static_cast<DeliveryFixture*>(userData);
	fixture.sentinelRuns.fetch_add(1, std::memory_order_relaxed);
	return static_cast<std::size_t>(endIndex - startIndex);
}

/// @brief Wait until a predicate holds, and say whether it came to hold within `patience`.
/// @details Delivery is done by the thread that closed the join, so unlike a budget window it needs nothing from the
///          base stream and a test may wait for it instead of pumping anything. Predicates count runs, never a
///          measurement a window reopen could zero.
template <typename TPredicate>
bool WaitFor(const TPredicate& holds, std::chrono::milliseconds patience) noexcept
{
	const auto deadline = std::chrono::steady_clock::now() + patience;
	while (!holds() && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return holds();
}

} // namespace

void TaskSystemTest::Prepare()
{
	AddTest("Empty Task", [this](TLogOut& ls)
	{
		Task task;
		if (task.HasDone())
		{
			ls << "Dummy task should not be set to done before it's enqueued." << lferr;
		}
	});

	AddTest("Task of size 0", [this](TLogOut& ls)
	{
		auto func = [](void*, std::size_t start, std::size_t end) -> std::size_t
		{
			auto log = Logger::Get("Size 0 Task");
			log.Out([&](auto& ls) { ls << "Range[" << (start + 1) << ", " << end << ')'; });

			return 1;
		};

		const TrackedTask trackedTask("TestTask", func, nullptr);
		auto& task = *trackedTask;
		if (task.HasDone())
		{
			ls << "The task should not be marked done before running." << lferr;
		}

		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();
		task.ReserveSubTasks(1);
		taskSys.Enqueue(task.GenerateSubTask(0, 0));
		WaitUntil([&task]() { return task.HasDone(); });

		if (!task.HasDone())
		{
			ls << "The task should be marked done after running." << lferr;
		}
	});

	AddTest("Bagel Problem", [this](TLogOut& ls)
	{
		constexpr std::size_t Count = 1000000;
		constexpr std::size_t NumSubJobs = 10;

		struct SplitSum final
		{
			std::array<std::atomic<double>, NumSubJobs> slots;
			std::atomic<std::size_t> nextSlot{0};
			std::atomic<unsigned> subJobRuns{0};
			std::atomic<unsigned> collatorRuns{0};
			std::atomic<double> collated{0.0};
		};

		SplitSum split;
		for (auto& slot : split.slots)
		{
			slot.store(0.0, std::memory_order_relaxed);
		}

		auto sumRange = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			SplitSum& state = *static_cast<SplitSum*>(userData);

			double partial = 0.0;
			for (std::size_t i = startIndex + 1; i <= endIndex; ++i)
			{
				double value = 1.0 / static_cast<double>(i);
				value *= value;
				partial += value;
			}

			const std::size_t slot = state.nextSlot.fetch_add(1, std::memory_order_relaxed) % NumSubJobs;
			state.slots[slot].store(partial, std::memory_order_relaxed);
			state.subJobRuns.fetch_add(1, std::memory_order_relaxed);

			return endIndex - startIndex;
		};

		auto collate = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			SplitSum& state = *static_cast<SplitSum*>(userData);

			double total = 0.0;
			for (const auto& slot : state.slots)
			{
				total += slot.load(std::memory_order_relaxed);
			}

			state.collated.store(total, std::memory_order_relaxed);
			state.collatorRuns.fetch_add(1, std::memory_order_relaxed);

			return endIndex - startIndex;
		};

		auto& taskSys = Engine::Get().GetTaskSystem();
		const TaskSystem::TIndex firstWorker = TaskSystem::GetIOTaskStreamIndex() + 1;
		const TaskSystem::TIndex chosenStreams[] = {firstWorker, firstWorker + 1};
		if (!taskSys.HasStream(chosenStreams[1]))
		{
			ls << "This engine has no second worker stream, so a split cannot be shown spreading." << lferr;
			return;
		}

		const TrackedTask collatorTask("BagelCollator", collate, &split);
		(*collatorTask).ReserveSubTasks(1);

		const TaskID splitID = taskSys.ParallelFor("BagelSplit", sumRange, &split, Count, NumSubJobs, chosenStreams,
												   std::size(chosenStreams), 0, collatorTask.id, chosenStreams[1]);

		ls << "Split over " << Count << " items into " << NumSubJobs << " sub-jobs on streams " << chosenStreams[0]
		   << " and " << chosenStreams[1] << ", collating into task record " << collatorTask.id.index << "." << lf;

		if (splitID.IsNull())
		{
			ls << "ParallelFor refused a well-formed split over two real streams." << lferr;
			return;
		}

		const bool collated = WaitFor([&split] { return split.collatorRuns.load() > 0; }, std::chrono::seconds(20));

		const double result = split.collated.load();
		constexpr double EulerAnswer = Pi * Pi / 6.0;

		ls << "Sub-jobs that ran: " << split.subJobRuns.load() << ", slots claimed: " << split.nextSlot.load()
		   << ", collator runs: " << split.collatorRuns.load() << ". Test Result = " << result
		   << ", Pi squared over 6 = " << EulerAnswer << lf;

		const double error = std::round(result - EulerAnswer);
		if (error > Epsilon)
		{
			ls << "The error exceeds limit. Error = " << error << ", and the collator ran " << split.collatorRuns.load()
			   << " time(s) over " << split.subJobRuns.load() << " sub-jobs." << lferr;
		}

		if (!collated)
		{
			ls << "The join closed on " << split.subJobRuns.load() << " of " << NumSubJobs
			   << " sub-jobs and the collator was never dispatched. An asynchronous join that never fires is the"
			   << " failure G5 warns about, and unlike BusyWait it is silent." << lferr;
		}

		if (split.subJobRuns.load() != NumSubJobs)
		{
			ls << "Expected " << NumSubJobs << " sub-jobs to run, got " << split.subJobRuns.load() << "." << lferr;
		}

		taskSys.ReleaseTask(splitID);
	});

	AddTest("Bagel Problem (Incremental Task)", [this](TLogOut& ls)
	{
		constexpr std::size_t Count = 1000000;
		constexpr std::size_t NumSubtasks = 5;
		constexpr std::size_t Increment = Count / NumSubtasks;

		double result = 0;

		auto func = [](void* userData, std::size_t start, std::size_t end) -> std::size_t
		{
			double taskResult = 0;

			auto incEnd = std::min(end, start + (Increment / 7));
			for (std::size_t i = start + 1; i <= incEnd; ++i)
			{
				double value = 1.0 / static_cast<double>(i);
				value *= value;
				taskResult += value;
			}

			auto* totalSumPtr = static_cast<double*>(userData);
			double& totalSum = *totalSumPtr;
			totalSum += static_cast<float>(taskResult);

			return incEnd - start;
		};

		const TrackedTask trackedTask("TestTask", func, &result);
		auto& task = *trackedTask;
		if (task.HasDone())
		{
			ls << "The task should not be marked done before running." << lferr;
		}

		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		task.ReserveSubTasks(NumSubtasks);
		for (std::size_t i = 0; i < Count; i += Increment)
		{
			taskSys.Enqueue(task.GenerateSubTask(i, i + Increment));
		}

		WaitUntil([&task]() { return task.HasDone(); });

		if (!task.HasDone())
		{
			ls << "The task should be marked done after running." << lferr;
		}

		constexpr double EulerAnswer = Pi * Pi / 6.0;
		ls << "Test Result = " << result << ", Pi/6 = " << EulerAnswer << lf;

		const double error = std::round(result - EulerAnswer);
		if (error > Epsilon)
		{
			ls << "The error exceeds limit. Error = " << error << lferr;
		}
	});

	AddTest("Both lanes serve their tasks", [this](TLogOut& ls)
	{
		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		const auto workerIndex = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerIndex))
		{
			ls << "No worker stream at index " << workerIndex << ", so no lane could be observed." << lferr;
			return;
		}

		auto& stream = taskSys.GetStream(workerIndex);

		// Polling with a deadline rather than waiting on the task: a task that never gets served must produce
		// a failed test, not a suite that stalls, which is the failure mode this test exists to detect.
		static std::atomic<unsigned> laneRuns{0};
		laneRuns.store(0, std::memory_order_relaxed);

		auto countRun = [](void*, std::size_t, std::size_t) -> std::size_t
		{
			laneRuns.fetch_add(1, std::memory_order_relaxed);
			return 1;
		};

		const TrackedTask trackedLane("LaneTask", countRun, nullptr);
		auto& laneTask = *trackedLane;

		constexpr unsigned fifoTasks = 3;
		laneTask.ReserveSubTasks(fifoTasks + 1);
		for (unsigned index = 0; index < fifoTasks; ++index)
		{
			stream.EnqueueFifo(laneTask.GenerateSubTask(index, index + 1));
		}

		stream.EnqueuePriority(laneTask.GenerateSubTask(fifoTasks, fifoTasks + 1));

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!laneTask.HasDone() && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		const auto runs = laneRuns.load(std::memory_order_relaxed);
		ls << stream.GetName().c_str() << " served " << runs << " of " << fifoTasks + 1
		   << " tasks across its two lanes." << lf;

		if (!laneTask.HasDone())
		{
			ls << "Only " << runs << " of " << fifoTasks + 1
			   << " tasks completed, so a lane is not draining or an unfinished task was dropped instead of"
			   << " returned to its lane." << lferr;
		}
	});

	AddTest("Stream charges a configured budget", [this](TLogOut& ls)
	{
		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		// The whole suite executes inside a task on the base stream - see UnitTestCollection.cpp's TestEnv
		// subtask - so the base stream is occupied until the suite ends and can never run anything a test
		// enqueues to it. The first worker stream is the nearest one that is genuinely idle.
		const auto workerIndex = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerIndex))
		{
			ls << "No worker stream at index " << workerIndex << ", so this test could not observe a stream at work."
			   << lferr;
			return;
		}

		auto& stream = taskSys.GetStream(workerIndex);
		ls << "Measuring " << stream.GetName().c_str() << ", which is idle while the suite runs elsewhere." << lf;

		if (!stream.MayTakeNewWork())
		{
			ls << "A stream with no configured budget refused work." << lferr;
		}

		if (stream.GetAccumulatedCPUTime() != std::chrono::nanoseconds::zero())
		{
			ls << "An unconfigured stream measured its tasks, paying a syscall for a number nothing reads." << lferr;
		}

		constexpr auto allowance = std::chrono::milliseconds(1);
		stream.RequestBudget(allowance);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		auto busyFunc = [](void*, std::size_t, std::size_t) -> std::size_t
		{
			unsigned long long sink = 0;
			for (unsigned long long counter = 0; counter < 50000000ULL; ++counter)
			{
				sink += counter % 7U;
			}

			return static_cast<std::size_t>(sink & 0xFFFFU);
		};

		const TrackedTask trackedBusy("BudgetTask", busyFunc, nullptr);
		auto& busyTask = *trackedBusy;
		busyTask.ReserveSubTasks(1);
		taskSys.Enqueue(workerIndex, busyTask.GenerateSubTask(0, 1));
		WaitUntil([&busyTask]() { return busyTask.HasDone(); });

		const auto charged = stream.GetAccumulatedCPUTime();
		ls << stream.GetName().c_str() << " charged "
		   << std::chrono::duration_cast<std::chrono::microseconds>(charged).count() << " us against a 1 ms allowance."
		   << lf;

		if (charged <= allowance)
		{
			ls << "The busy task charged " << std::chrono::duration_cast<std::chrono::microseconds>(charged).count()
			   << " us; the stream is not measuring the tasks it runs. If it charged nothing, check that this task "
			   << "still runs on the worker stream rather than the base." << lferr;
		}

		if (stream.MayTakeNewWork())
		{
			ls << "A stream that has spent its allowance still reported that it may take more work." << lferr;
		}

		stream.RequestBudget(std::chrono::duration<double>{});

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		if (!stream.MayTakeNewWork())
		{
			ls << "Restoring the unlimited allowance did not restore willingness to take work." << lferr;
		}
	});

	AddTest("The base stream is named Base, and the thread driving the engine is not called that", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();

		const auto baseName = taskSys.GetStreamName(TaskSystem::GetBaseTaskStreamIndex());
		const auto ioName = taskSys.GetStreamName(TaskSystem::GetIOTaskStreamIndex());

		// This test runs inside a task, and the suite's task is a work item on the base stream, so the thread
		// running it is that stream's thread and must report exactly that.
		ls << "Stream 0 is named \"" << baseName.c_str() << "\", stream 1 is \"" << ioName.c_str()
		   << "\", this thread reports stream index " << TaskSystem::GetCurrentStreamIndex() << " named \""
		   << TaskSystem::GetCurrentThreadName().c_str() << "\" and IsBaseThread = " << TaskSystem::IsBaseThread()
		   << '.' << lf;

		if (baseName != StaticString("Base"))
		{
			ls << "Stream 0 is named \"" << baseName.c_str() << "\". It is the base stream, so the name that says"
			   << " so belongs to it and to nothing else." << lferr;
		}
		if (ioName != StaticString("IO"))
		{
			ls << "Stream 1 is named \"" << ioName.c_str() << "\", which is what the rename was done next to." << lferr;
		}
		if (StaticString(TaskSystem::EngineLoopThreadName) != StaticString("EngineLoop"))
		{
			ls << "The thread driving Engine::Run is named \"" << TaskSystem::EngineLoopThreadName << "\" rather"
			   << " than its own name, so two threads would log under one name again." << lferr;
		}
		if (!TaskSystem::IsBaseThread())
		{
			ls << "A test runs inside a task on the base stream, and that thread does not report itself as such -"
			   << " so the predicate cannot be trusted to mean what it says anywhere." << lferr;
		}
		if (TaskSystem::GetCurrentStreamIndex() != TaskSystem::GetBaseTaskStreamIndex())
		{
			ls << "The base stream's thread reports stream index " << TaskSystem::GetCurrentStreamIndex() << '.'
			   << lferr;
		}
	});

	AddTest("A thread that was never given a stream does not claim to have one", [this](TLogOut& ls)
	{
		static_assert(TaskSystem::NonStreamIndex >= TaskStreamAffinity::GetNumBits());

		std::atomic<TaskSystem::TIndex> foreignIndex{0};
		std::atomic<bool> foreignClaimsBase{true};
		std::atomic<bool> foreignClaimsIO{true};

		std::thread probe([&]()
		{
			foreignIndex = TaskSystem::GetCurrentStreamIndex();
			foreignClaimsBase = TaskSystem::IsBaseThread();
			foreignClaimsIO = TaskSystem::IsIOThread();
		});
		probe.join();

		ls << "A thread nobody created as a stream reports index " << foreignIndex.load()
		   << ", IsBaseThread = " << foreignClaimsBase.load() << ", IsIOThread = " << foreignClaimsIO.load()
		   << ", and the affinity mask"
		   << " holds " << TaskStreamAffinity::GetNumBits() << " bits." << lf;

		if (foreignIndex.load() != TaskSystem::NonStreamIndex)
		{
			ls << "A thread that has no stream reports index " << foreignIndex.load() << ". With index 0 as the"
			   << " default every thread the application creates claimed to be the base stream." << lferr;
		}
		if (foreignClaimsBase.load())
		{
			ls << "A thread that has no stream claimed to be the base thread, so that predicate cannot gate"
			   << " anything - including the assert in BuildStreams, which passed on every thread." << lferr;
		}
		if (foreignClaimsIO.load())
		{
			ls << "A thread that has no stream claimed to be the IO thread." << lferr;
		}
		if (foreignIndex.load() < TaskStreamAffinity::GetNumBits())
		{
			ls << "The no-stream index " << foreignIndex.load() << " is inside the " << TaskStreamAffinity::GetNumBits()
			   << "-bit affinity mask, so a thread that is not a stream would be recorded as one - and the general"
			   << " queue would charge its sightings to whichever stream shares the value." << lferr;
		}
	});

	// The witnesses here are what a stream does with a work item, never what its budget counter reads: advancing a
	// window zeroes that counter, so reading it to prove the mechanism would let the mechanism erase its own
	// evidence. Measured as exactly that - a charge of 0 us reported for a task that had just spent 222 ms.


	// What is watched here is what a stream does with work, never what its budget counter reads: advancing a window
	// zeroes that counter, so a test that read it to prove the mechanism would watch the mechanism erase its own
	// evidence. Measured as exactly that - 0 us reported for a task that had just spent 222 ms.


	// What is watched here is what a stream does with work, never what its budget counter reads, because a window
	// advance zeroes that counter: reading it to prove the mechanism would mean watching the mechanism erase its own
	// evidence. Measured as exactly that - 0 us reported for a task that had just spent 222 ms. The same reason sets
	// the shape of the waits below. Advancing windows keeps a stream reopened, so a test that advanced them while
	// looking for a refusal would never see one: the pass is what un-shuts a stream, and the state under test is
	// shut. Everything here therefore advances windows only until a task is confirmed to be running, and then leaves
	// them alone long enough for that task to finish.
	AddTest("A stream with a spent allowance declines the general queue and resumes after a window", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const auto workerIndex = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerIndex))
		{
			ls << "No worker stream at index " << workerIndex << ", so no stream could be shown throttling itself."
			   << lferr;
			return;
		}

		auto& stream = taskSys.GetStream(workerIndex);

		// Read the pass counter before this test touches anything: it is the witness that TaskStream's loop calls
		// the pass by itself. A test that advanced the first window on its own proves nothing about that wiring.
		const auto windowsSoFar = taskSys.GetNumBudgetWindowsAdvanced();
		ls << "The base stream's own loop had advanced " << windowsSoFar << " window(s) before this test ran." << lf;

		if (windowsSoFar == 0)
		{
			ls << "No budget window has closed in the ~55 s of engine run time before this test, so the base stream's"
			   << " loop never calls RunBudgetWindowPass, and an allowance configured anywhere in the engine stays"
			   << " spent for the life of the process." << lferr;
		}

		auto BurnCpu = [](void* userData, TIndex startIndex, TIndex endIndex) -> std::size_t
		{
			static_cast<std::atomic<unsigned>*>(userData)->fetch_add(1, std::memory_order_relaxed);
			volatile unsigned long long sink = 0;
			for (unsigned long long counter = 0; counter < 5000000ULL; ++counter)
			{
				sink += counter % 7U;
			}

			return static_cast<std::size_t>(endIndex - startIndex);
		};

		constexpr auto allowance = std::chrono::milliseconds(1);
		stream.RequestBudget(allowance);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		std::atomic<unsigned> burnRuns{0};
		const TrackedTask burn("SpendAllowance", BurnCpu, &burnRuns);
		(*burn).ReserveSubTasks(1);
		taskSys.Enqueue(workerIndex, (*burn).GenerateSubTask(0, 1));

		const auto refusalsBefore = stream.GetGeneralQueueRefusalCount();

		if (!AdvanceWindowsUntil(taskSys, [&burnRuns] { return burnRuns.load() > 0; }, std::chrono::seconds(5)))
		{
			ls << stream.GetName().c_str() << " never ran a task queued to its own lane within 5 s of windows"
			   << " advancing on every poll, so the stream never got as far as spending anything. An exhausted round"
			   << " that no reopen ends is the latch this pass exists to break." << lferr;
			stream.RequestBudget(std::chrono::duration<double>{});

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			return;
		}

		// The task is running and no window advance is left pending beyond the one that started it, so its charge
		// survives: it lands when the stream closes the task, and the stream's next round the loop is the refusal.
		// Measured at roughly 20 ms of CPU for this task, against the 200 ms waited here.
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		const auto refusals = stream.GetGeneralQueueRefusalCount() - refusalsBefore;

		if (refusals == 0)
		{
			ls << stream.GetName().c_str() << " charged a 1 ms allowance and took general queue work for 200 ms"
			   << " anyway. An allowance that changes no decision is billed per task as a measurement of nothing -"
			   << " the syscall is paid and the answer is ignored." << lferr;
		}
		else
		{
			ls << stream.GetName().c_str() << " declined the general queue " << refusals << " time(s) with its"
			   << " allowance spent." << lf;
		}

		// The same stream must resume the moment a window advances it: refusing forever is the defect the pass is
		// for, and it is reachable from here, since a stream that never reopened would still be shut right now.
		const bool resumed =
				AdvanceWindowsUntil(taskSys, [&stream] { return stream.MayTakeNewWork(); }, std::chrono::seconds(5));

		if (!resumed)
		{
			ls << stream.GetName().c_str() << " is still shut after 5 s of windows advancing. That is the permanent"
			   << " latch: an allowance spent once stays spent for the life of the process." << lferr;
		}
		else
		{
			ls << stream.GetName().c_str() << " resumed on its own thread at window "
			   << taskSys.GetNumBudgetWindowsAdvanced() << ", having declined " << refusals << " refusal(s) while"
			   << " shut." << lf;
		}

		const auto drainDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!(*burn).HasDone() && std::chrono::steady_clock::now() < drainDeadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		stream.RequestBudget(std::chrono::duration<double>{});

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		if (!stream.MayTakeNewWork())
		{
			ls << "Restoring the unlimited allowance did not restore willingness to take work." << lferr;
		}
	});


	AddTest("A stream with no allowance never declines general work", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		if (!taskSys.HasStream(TaskSystem::GetIOTaskStreamIndex()))
		{
			ls << "No IO stream, so no unlimited stream could be observed." << lferr;
			return;
		}

		auto& ioStream = taskSys.GetStream(TaskSystem::GetIOTaskStreamIndex());
		const auto refusals = ioStream.GetGeneralQueueRefusalCount();

		ls << ioStream.GetName().c_str() << " has an unlimited allowance and has declined general work " << refusals
		   << " time(s)." << lf;

		if (refusals != 0)
		{
			ls << "A stream with no allowance refused general work, so the gate is reading something other than "
			   << "an unspent allowance - or an unlimited stream is being measured and charged anyway." << lferr;
		}
	});


	AddTest("A finished task hands its outcome to the successor on the stream it named", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const auto producerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		const auto successorStream = producerStream + 1;
		if (!taskSys.HasStream(successorStream))
		{
			ls << "This engine has no stream " << successorStream << ", so a delivery cannot be shown crossing from"
			   << " one stream to another." << lferr;
			return;
		}

		DeliveryFixture producer;
		producer.taskSystem = &taskSys;
		producer.destination = static_cast<std::uint8_t>(producerStream);

		DeliveryFixture successor;
		successor.taskSystem = &taskSys;

		const TrackedTask producerTask("DeliverProducer", RunStageOne, &producer);
		const TrackedTask successorTask("DeliverSuccessor", RunStageThree, &successor);
		const TrackedTask sentinelTask("DeliverSentinel", RunSentinel, &producer);
		producer.self = producerTask.id;
		successor.self = successorTask.id;

		producer.destination = static_cast<std::uint8_t>(successorStream);
		(*producerTask).ReserveSubTasks(1);
		(*successorTask).ReserveSubTasks(1);
		(*sentinelTask).ReserveSubTasks(1);
		taskSys.SetSuccessor(producer.self, successor.self);

		taskSys.Enqueue(producerStream, (*producerTask).GenerateSubTask(0, 1));
		taskSys.Enqueue(producerStream, (*sentinelTask).GenerateSubTask(0, 1));

		if (!WaitFor([&producer] { return producer.sentinelRuns.load() > 0; }, std::chrono::seconds(5)))
		{
			ls << "The sentinel queued behind the producer never ran within 5 s, so the producer's work item never"
			   << " left stream " << producerStream << " and no delivery could have been attempted." << lferr;
			return;
		}

		const bool delivered =
				WaitFor([&successor] { return successor.stageThreeRuns.load() > 0; }, std::chrono::seconds(5));

		ls << "Producer ran " << producer.stageOneRuns.load() << " time(s); successor on stream "
		   << static_cast<unsigned int>(producer.destination) << " ran " << successor.stageThreeRuns.load()
		   << " time(s), reading kind " << static_cast<unsigned int>(successor.deliveredKind.load())
		   << " and payload byte " << static_cast<int>(successor.deliveredPayload.load()) << "." << lf;

		if (!delivered)
		{
			ls << "The producer closed its join with a successor recorded and its packet naming stream "
			   << static_cast<unsigned int>(producer.destination) << ", and that successor was never dispatched."
			   << " This is the whole of section 6.1 not happening." << lferr;
		}
		else if (successor.deliveredKind.load() != ResultPacket::FirstApplicationKind ||
				 successor.deliveredPayload.load() != 0xA5)
		{
			ls << "The successor ran but its packet is not the outcome the producer wrote: kind "
			   << static_cast<unsigned int>(successor.deliveredKind.load()) << " byte "
			   << static_cast<int>(successor.deliveredPayload.load()) << ", expected kind "
			   << static_cast<unsigned int>(ResultPacket::FirstApplicationKind)
			   << " and byte 165. Dispatch copying the packet across is the delivery, not a courtesy." << lferr;
		}

		if (producer.stageOneRuns.load() != 1)
		{
			ls << "The producer ran " << producer.stageOneRuns.load() << " time(s), and a task queued once runs once"
			   << " unless something re-dispatched it." << lferr;
		}
	});

	AddTest("Outcomes chain: a successor that produces in turn dispatches its own successor", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const auto firstStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		const auto lastStream = firstStream + 1;
		if (!taskSys.HasStream(lastStream))
		{
			ls << "This engine has no stream " << lastStream << ", so a two-link chain cannot cross streams." << lferr;
			return;
		}

		DeliveryFixture first;
		DeliveryFixture middle;
		DeliveryFixture last;
		first.taskSystem = &taskSys;
		middle.taskSystem = &taskSys;
		last.taskSystem = &taskSys;

		const TrackedTask firstTask("ChainFirst", RunStageOne, &first);
		const TrackedTask middleTask("ChainMiddle", RunStageTwo, &middle);
		const TrackedTask lastTask("ChainLast", RunStageThree, &last);
		const TrackedTask sentinelTask("ChainSentinel", RunSentinel, &first);
		first.self = firstTask.id;
		middle.self = middleTask.id;
		last.self = lastTask.id;
		first.next = middle.self;
		middle.next = last.self;
		first.destination = static_cast<std::uint8_t>(firstStream);
		middle.destination = static_cast<std::uint8_t>(lastStream);

		(*firstTask).ReserveSubTasks(1);
		(*middleTask).ReserveSubTasks(1);
		(*lastTask).ReserveSubTasks(1);
		(*sentinelTask).ReserveSubTasks(1);
		taskSys.SetSuccessor(first.self, middle.self);

		taskSys.Enqueue(firstStream, (*firstTask).GenerateSubTask(0, 1));
		taskSys.Enqueue(firstStream, (*sentinelTask).GenerateSubTask(0, 1));

		WaitFor([&first] { return first.sentinelRuns.load() > 0; }, std::chrono::seconds(5));
		const bool middleRan = WaitFor([&middle] { return middle.stageTwoRuns.load() > 0; }, std::chrono::seconds(5));
		const bool lastRan = WaitFor([&last] { return last.stageThreeRuns.load() > 0; }, std::chrono::seconds(5));

		ls << "Links that ran: first " << first.stageOneRuns.load() << ", middle " << middle.stageTwoRuns.load()
		   << ", last " << last.stageThreeRuns.load() << "; last read byte "
		   << static_cast<int>(last.deliveredPayload.load()) << "." << lf;

		if (!middleRan)
		{
			ls << "The first link closed its join and the middle was never dispatched, so the chain has one link."
			   << lferr;
		}
		else if (!lastRan)
		{
			ls << "The middle link ran, recorded its own successor and produced an outcome, and that successor was"
			   << " never dispatched. A successor dispatched by a task the engine itself dispatched is the case R9"
			   << " is for: if only the first link's outcome ever routes, chains are not supported, they only look"
			   << " supported until a second link is needed." << lferr;
		}
		else if (last.deliveredPayload.load() != 0x5A)
		{
			ls << "The last link ran but carries byte " << static_cast<int>(last.deliveredPayload.load())
			   << " instead of the 90 the middle wrote, so a link forwarded something other than its own outcome."
			   << lferr;
		}
	});

	AddTest("A task that recorded no successor wakes nobody, and does not run itself again", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const auto workerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerStream + 1))
		{
			ls << "This engine has no stream " << workerStream + 1
			   << ", so nothing could be shown staying undispatched." << lferr;
			return;
		}

		DeliveryFixture producer;
		producer.taskSystem = &taskSys;
		producer.destination = static_cast<std::uint8_t>(workerStream + 1);

		DeliveryFixture bystander;
		bystander.taskSystem = &taskSys;

		const TrackedTask producerTask("NoSuccessorProducer", RunStageOne, &producer);
		const TrackedTask bystanderTask("NoSuccessorBystander", RunStageTwo, &bystander);
		const TrackedTask sentinelTask("NoSuccessorSentinel", RunSentinel, &producer);
		producer.self = producerTask.id;
		bystander.self = bystanderTask.id;

		(*producerTask).ReserveSubTasks(1);
		(*bystanderTask).ReserveSubTasks(1);
		(*sentinelTask).ReserveSubTasks(1);

		taskSys.Enqueue(workerStream, (*producerTask).GenerateSubTask(0, 1));
		taskSys.Enqueue(workerStream, (*sentinelTask).GenerateSubTask(0, 1));

		if (!WaitFor([&producer] { return producer.sentinelRuns.load() > 0; }, std::chrono::seconds(5)))
		{
			ls << "The sentinel never ran, so the producer's item never completed and nothing here was tested."
			   << lferr;
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		ls << "Producer ran " << producer.stageOneRuns.load() << " time(s), the bystander it never named ran "
		   << bystander.stageTwoRuns.load() << " time(s)." << lf;

		if (bystander.stageTwoRuns.load() != 0)
		{
			ls << "A task with no successor recorded dispatched one anyway. R16's fire-and-forget job is the common"
			   << " shape, and if finishing wakes tasks nobody recorded, every fire-and-forget job becomes a dispatch"
			   << " with an invented target." << lferr;
		}

		if (producer.stageOneRuns.load() != 1)
		{
			ls << "The producer ran " << producer.stageOneRuns.load() << " time(s) from one queueing. A task that"
			   << " wakes itself when it has no successor is a loop that never ends, and nothing downstream can tell"
			   << " it apart from slow work." << lferr;
		}
	});

	AddTest("A successor with no stream named in the packet is refused out loud, not guessed", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const auto workerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerStream))
		{
			ls << "No worker stream, so no delivery could be attempted at all." << lferr;
			return;
		}

		DeliveryFixture producer;
		producer.taskSystem = &taskSys;

		DeliveryFixture successor;
		successor.taskSystem = &taskSys;

		const TrackedTask producerTask("NoStreamProducer", RunStageOne, &producer);
		const TrackedTask successorTask("NoStreamSuccessor", RunStageTwo, &successor);
		const TrackedTask sentinelTask("NoStreamSentinel", RunSentinel, &producer);
		producer.self = producerTask.id;
		successor.self = successorTask.id;

		(*producerTask).ReserveSubTasks(1);
		(*successorTask).ReserveSubTasks(1);
		(*sentinelTask).ReserveSubTasks(1);
		taskSys.SetSuccessor(producer.self, successor.self);

		taskSys.Enqueue(workerStream, (*producerTask).GenerateSubTask(0, 1));
		taskSys.Enqueue(workerStream, (*sentinelTask).GenerateSubTask(0, 1));

		if (!WaitFor([&producer] { return producer.sentinelRuns.load() > 0; }, std::chrono::seconds(5)))
		{
			ls << "The sentinel never ran, so the delivery was never attempted." << lferr;
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		ls << "The producer produced a result naming no stream, and its successor ran " << successor.stageTwoRuns.load()
		   << " time(s). An error naming the pair is the only output of this test." << lf;

		if (successor.stageTwoRuns.load() != 0)
		{
			ls << "A successor was dispatched although its packet named no stream, so the engine picked a stream for"
			   << " a routing decision its caller never made." << lferr;
		}
	});

	AddTest("A successor addressed to a stream this engine does not have is refused", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const auto workerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerStream))
		{
			ls << "No worker stream, so the producing half of this test cannot run." << lferr;
			return;
		}

		DeliveryFixture producer;
		producer.taskSystem = &taskSys;
		producer.destination = 0xFE;

		DeliveryFixture successor;
		successor.taskSystem = &taskSys;

		const TrackedTask producerTask("BogusStreamProducer", RunStageOne, &producer);
		const TrackedTask successorTask("BogusStreamSuccessor", RunStageTwo, &successor);
		const TrackedTask sentinelTask("BogusStreamSentinel", RunSentinel, &producer);
		producer.self = producerTask.id;
		successor.self = successorTask.id;

		(*producerTask).ReserveSubTasks(1);
		(*successorTask).ReserveSubTasks(1);
		(*sentinelTask).ReserveSubTasks(1);
		taskSys.SetSuccessor(producer.self, successor.self);

		taskSys.Enqueue(workerStream, (*producerTask).GenerateSubTask(0, 1));
		taskSys.Enqueue(workerStream, (*sentinelTask).GenerateSubTask(0, 1));

		if (!WaitFor([&producer] { return producer.sentinelRuns.load() > 0; }, std::chrono::seconds(5)))
		{
			ls << "The sentinel never ran, so the delivery was never attempted." << lferr;
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		ls << "Stream 254 does not exist, and the successor ran " << successor.stageTwoRuns.load() << " time(s)." << lf;

		if (successor.stageTwoRuns.load() != 0)
		{
			ls << "A destination of 254 reached a queue, so the byte is used as an index unchecked against the stream"
			   << " count - a wild write into an Array the engine does not own." << lferr;
		}
	});

	AddTest("A successor released while its producer runs is reported and never dispatched", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const auto workerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerStream))
		{
			ls << "No worker stream, so no delivery could be attempted." << lferr;
			return;
		}

		DeliveryFixture producer;
		producer.taskSystem = &taskSys;
		producer.destination = static_cast<std::uint8_t>(workerStream);

		DeliveryFixture successor;
		successor.taskSystem = &taskSys;

		const TaskID abandonedID = taskSys.CreateTask("AbandonedSuccessor", RunStageTwo, &successor);
		successor.self = abandonedID;
		(*taskSys.FindTask(abandonedID)).ReserveSubTasks(1);
		taskSys.ReleaseTask(abandonedID);

		const TrackedTask producerTask("ReleasedSuccessorProducer", RunStageOne, &producer);
		const TrackedTask sentinelTask("ReleasedSuccessorSentinel", RunSentinel, &producer);
		producer.self = producerTask.id;
		producer.next = abandonedID;

		(*producerTask).ReserveSubTasks(1);
		(*sentinelTask).ReserveSubTasks(1);
		taskSys.SetSuccessor(producer.self, abandonedID);

		taskSys.Enqueue(workerStream, (*producerTask).GenerateSubTask(0, 1));
		taskSys.Enqueue(workerStream, (*sentinelTask).GenerateSubTask(0, 1));

		const bool barrierRan =
				WaitFor([&producer] { return producer.sentinelRuns.load() > 0; }, std::chrono::seconds(5));

		ls << "The task was released at record " << abandonedID.index << " generation " << abandonedID.generation
		   << " before its producer ran; the sentinel completed the barrier: " << (barrierRan ? "yes" : "no")
		   << ", and the released task's runnable ran " << successor.stageTwoRuns.load() << " time(s)." << lf;

		if (successor.stageTwoRuns.load() != 0)
		{
			ls << "A task released before its producer finished still ran, so the registry handed out a record that"
			   << " was on the free list - or its slot had been taken by a different task, which then ran someone"
			   << " else's work under the abandoned identity." << lferr;
		}

		if (!barrierRan)
		{
			ls << "The sentinel never ran, so this test observed nothing." << lferr;
		}
	});

	AddTest("A successor that reserved no subtask cannot be dispatched and says why", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const auto workerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerStream))
		{
			ls << "No worker stream, so no delivery could be attempted." << lferr;
			return;
		}

		DeliveryFixture producer;
		producer.taskSystem = &taskSys;
		producer.destination = static_cast<std::uint8_t>(workerStream);

		DeliveryFixture successor;
		successor.taskSystem = &taskSys;

		const TrackedTask producerTask("UnreservedProducer", RunStageOne, &producer);
		const TrackedTask successorTask("UnreservedSuccessor", RunStageTwo, &successor);
		const TrackedTask sentinelTask("UnreservedSentinel", RunSentinel, &producer);
		producer.self = producerTask.id;
		successor.self = successorTask.id;

		(*producerTask).ReserveSubTasks(1);
		(*sentinelTask).ReserveSubTasks(1);
		taskSys.SetSuccessor(producer.self, successor.self);

		taskSys.Enqueue(workerStream, (*producerTask).GenerateSubTask(0, 1));
		taskSys.Enqueue(workerStream, (*sentinelTask).GenerateSubTask(0, 1));

		if (!WaitFor([&producer] { return producer.sentinelRuns.load() > 0; }, std::chrono::seconds(5)))
		{
			ls << "The sentinel never ran, so the delivery was never attempted." << lferr;
			return;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		ls << "The successor reserved nothing under R29 and ran " << successor.stageTwoRuns.load() << " time(s)." << lf;

		if (successor.stageTwoRuns.load() != 0)
		{
			ls << "A successor with no reserved subtask was dispatched. Its join counter then never reaches a count"
			   << " it was never given, so the task runs and closes nothing - the one shape of bug that looks like"
			   << " working code and hangs on the next link." << lferr;
		}
	});

	AddTest("A split queues no empty work item and counts only what it queued", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const TaskSystem::TIndex workerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerStream))
		{
			ls << "No worker stream, so no split could be queued at all." << lferr;
			return;
		}

		struct SplitShape final
		{
			std::atomic<unsigned> runs{0};
			std::atomic<unsigned> emptyRanges{0};
			std::atomic<unsigned> collatorRuns{0};
		};

		SplitShape shape;

		auto countRange = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			SplitShape& state = *static_cast<SplitShape*>(userData);
			state.runs.fetch_add(1, std::memory_order_relaxed);
			if (endIndex <= startIndex)
			{
				state.emptyRanges.fetch_add(1, std::memory_order_relaxed);
			}

			return endIndex > startIndex ? endIndex - startIndex : 1;
		};

		auto noteCollator = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			static_cast<SplitShape*>(userData)->collatorRuns.fetch_add(1, std::memory_order_relaxed);
			return endIndex - startIndex;
		};

		const TaskSystem::TIndex oneStream[] = {workerStream};
		const TrackedTask collatorTask("ClampCollator", noteCollator, &shape);
		(*collatorTask).ReserveSubTasks(1);

		constexpr TaskSystem::TIndex NumItems = 3;
		const TaskID splitID = taskSys.ParallelFor("ClampSplit", countRange, &shape, NumItems, 10, oneStream,
												   std::size(oneStream), 0, collatorTask.id, workerStream);

		if (splitID.IsNull())
		{
			ls << "ParallelFor refused a split of 3 items into 10 sub-jobs, which is a clamp, not a refusal." << lferr;
			return;
		}

		const bool closed = WaitFor([&shape] { return shape.collatorRuns.load() > 0; }, std::chrono::seconds(5));
		const Task* splitTask = taskSys.FindTask(splitID);
		const unsigned reserved = splitTask != nullptr ? static_cast<unsigned>((*splitTask).NumSubTasks()) : 0;

		ls << "Three items asked for in ten sub-jobs produced " << shape.runs.load() << " work item(s) covering the"
		   << " job, the join was declared as " << reserved << " sub-task(s), " << shape.emptyRanges.load()
		   << " of them covered an empty range, and the collator ran " << shape.collatorRuns.load() << " time(s)."
		   << lf;

		if (!closed)
		{
			ls << "The join never closed, so the clamped count and the number of items queued disagree - which is"
			   << " the one disagreement R29 makes fatal." << lferr;
		}

		if (shape.runs.load() != NumItems || reserved != NumItems)
		{
			ls << "Expected exactly " << NumItems << " items covering three units of work, got " << shape.runs.load()
			   << " item(s) and a join of " << reserved << "." << lferr;
		}

		if (shape.emptyRanges.load() != 0)
		{
			ls << shape.emptyRanges.load() << " work item(s) ran over an empty range, each reporting in for a unit"
			   << " of work that does not exist." << lferr;
		}

		taskSys.ReleaseTask(splitID);
	});

	AddTest("A split larger than the join counter can count is cut down, not truncated", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const TaskSystem::TIndex workerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerStream))
		{
			ls << "No worker stream, so no split could be queued." << lferr;
			return;
		}

		struct BigSplit final
		{
			std::atomic<unsigned> runs{0};
			std::atomic<unsigned> emptyRanges{0};
			std::atomic<unsigned> collatorRuns{0};
			std::atomic<unsigned> itemsCovered{0};
		};

		BigSplit big;

		auto countRange = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			BigSplit& state = *static_cast<BigSplit*>(userData);
			state.runs.fetch_add(1, std::memory_order_relaxed);
			if (endIndex <= startIndex)
			{
				state.emptyRanges.fetch_add(1, std::memory_order_relaxed);
				return 1;
			}

			state.itemsCovered.fetch_add(static_cast<unsigned>(endIndex - startIndex), std::memory_order_relaxed);
			return endIndex - startIndex;
		};

		auto noteCollator = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			static_cast<BigSplit*>(userData)->collatorRuns.fetch_add(1, std::memory_order_relaxed);
			return endIndex - startIndex;
		};

		const TaskSystem::TIndex oneStream[] = {workerStream};
		const TrackedTask collatorTask("TruncateCollator", noteCollator, &big);
		(*collatorTask).ReserveSubTasks(1);

		constexpr TaskSystem::TIndex NumItems = 1000;
		constexpr TaskSystem::TIndex AskedSubJobs = 300;
		const TaskID splitID = taskSys.ParallelFor("TruncateSplit", countRange, &big, NumItems, AskedSubJobs, oneStream,
												   std::size(oneStream), 0, collatorTask.id, workerStream);

		const bool closed = WaitFor([&big] { return big.collatorRuns.load() > 0; }, std::chrono::seconds(10));
		const Task* splitTask = taskSys.FindTask(splitID);
		const unsigned reserved = splitTask != nullptr ? static_cast<unsigned>((*splitTask).NumSubTasks()) : 0;

		ls << "A split of " << NumItems << " items into " << AskedSubJobs << " sub-jobs was declared as " << reserved
		   << " sub-task(s) (the counter is " << Task::MaxNumSubTasks << " at most), ran " << big.runs.load()
		   << " item(s) covering " << big.itemsCovered.load() << " of " << NumItems << " units, of which "
		   << big.emptyRanges.load() << " were empty, and the collator ran " << big.collatorRuns.load()
		   << " time(s). Join closed: " << (closed ? "yes" : "no") << "." << lf;

		if (reserved != Task::MaxNumSubTasks)
		{
			ls << "The join was declared as " << reserved << ", which is not " << Task::MaxNumSubTasks
			   << ". A count that a uint8_t cannot hold does not fail to compile - it silently becomes the low eight"
			   << " bits, and the join then closes while most of the work is still queued." << lferr;
		}

		if (big.runs.load() != reserved)
		{
			ls << big.runs.load() << " work items were queued against a join of " << reserved
			   << ", so the number that decides when the join closes is not the number queued." << lferr;
		}

		if (big.itemsCovered.load() != NumItems)
		{
			ls << "The sub-jobs covered " << big.itemsCovered.load() << " of " << NumItems
			   << " items, so clamping the split did not keep the whole job covered." << lferr;
		}

		if (big.emptyRanges.load() != 0)
		{
			ls << big.emptyRanges.load() << " item(s) covered an empty range." << lferr;
		}

		if (!closed)
		{
			ls << "The join never closed." << lferr;
		}

		taskSys.ReleaseTask(splitID);
	});

	AddTest("A split naming a stream the engine does not have is refused and creates nothing", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();

		std::atomic<unsigned> runs{0};

		auto countRun = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			static_cast<std::atomic<unsigned>*>(userData)->fetch_add(1, std::memory_order_relaxed);
			return endIndex > startIndex ? endIndex - startIndex : 1;
		};

		const TaskSystem::TIndex bogusStreams[] = {
				static_cast<TaskSystem::TIndex>(TaskSystem::GetIOTaskStreamIndex() + 60000)};
		const TaskID refused =
				taskSys.ParallelFor("BogusStreamSplit", countRun, &runs, 8, 4, bogusStreams, std::size(bogusStreams));

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		ls << "A split naming stream " << bogusStreams[0] << " returned a " << (refused.IsNull() ? "null" : "live")
		   << " task and ran " << runs.load() << " time(s)." << lf;

		if (!refused.IsNull())
		{
			ls << "A split onto a stream that does not exist came back with a live task, so the check on stream"
			   << " indices is not there and the enqueue used an index outside the array of streams." << lferr;
			taskSys.ReleaseTask(refused);
		}

		if (runs.load() != 0)
		{
			ls << "The refused split ran " << runs.load() << " time(s) anyway." << lferr;
		}
	});

	AddTest("Asking for streams picks workers and never the base stream or the IO stream", [this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();

		struct WhereRan final
		{
			std::array<std::atomic<int>, 64> streamOfSubJob;
			std::atomic<std::size_t> nextSlot{0};
			std::atomic<unsigned> collatorRuns{0};
		};

		WhereRan where;
		for (auto& slot : where.streamOfSubJob)
		{
			slot.store(-1, std::memory_order_relaxed);
		}

		auto noteStream = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			WhereRan& state = *static_cast<WhereRan*>(userData);
			const std::size_t slot =
					state.nextSlot.fetch_add(1, std::memory_order_relaxed) % state.streamOfSubJob.size();
			state.streamOfSubJob[slot].store(static_cast<int>(TaskSystem::GetCurrentStreamIndex()),
											 std::memory_order_relaxed);
			return endIndex > startIndex ? endIndex - startIndex : 1;
		};

		auto noteCollator = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			static_cast<WhereRan*>(userData)->collatorRuns.fetch_add(1, std::memory_order_relaxed);
			return endIndex - startIndex;
		};

		const TrackedTask collatorTask("VariantCollator", noteCollator, &where);
		(*collatorTask).ReserveSubTasks(1);

		constexpr TaskSystem::TIndex NumSubJobs = 8;
		const TaskID splitID = taskSys.ParallelFor("VariantSplit", noteStream, &where, 64, NumSubJobs, 999, 0,
												   collatorTask.id, TaskSystem::GetIOTaskStreamIndex() + 1);

		const bool closed = WaitFor([&where] { return where.collatorRuns.load() > 0; }, std::chrono::seconds(10));

		unsigned distinctUsed = 0;
		unsigned onBaseOrIO = 0;
		int highestStream = -1;
		for (const auto& slot : where.streamOfSubJob)
		{
			const int ran = slot.load(std::memory_order_relaxed);
			if (ran < 0)
			{
				continue;
			}

			++distinctUsed;
			highestStream = ran > highestStream ? ran : highestStream;
			if (static_cast<TaskSystem::TIndex>(ran) <= TaskSystem::GetIOTaskStreamIndex())
			{
				++onBaseOrIO;
			}
		}

		ls << "Asking for 999 streams placed " << NumSubJobs << " sub-jobs across " << distinctUsed
		   << " distinct stream(s), the highest being index " << highestStream << ", of which " << onBaseOrIO
		   << " landed on the base or IO stream. Join closed: " << (closed ? "yes" : "no") << "." << lf;

		if (!closed)
		{
			ls << "The split asked for 999 streams and its join never closed within 10 s, so some part of it went"
			   << " somewhere that does not pump." << lferr;
		}

		if (onBaseOrIO != 0)
		{
			ls << onBaseOrIO << " sub-job(s) ran on the base or IO stream. R10 keeps that placement expressible for"
			   << " a caller that names streams; an engine choosing them on its own must never pick either - R8"
			   << " exists because blocking them stalls the engine." << lferr;
		}

		if (distinctUsed != NumSubJobs)
		{
			ls << "Expected " << NumSubJobs << " sub-jobs to report where they ran, got " << distinctUsed << "."
			   << lferr;
		}

		taskSys.ReleaseTask(splitID);
	});

	AddTest("An item that returns part of its range resumes at the index it stopped at", [this](auto& ls)
	{
		struct ProgressLog
		{
			std::size_t calls = 0;
			std::size_t startSum = 0;
		};

		auto func = [](void* userData, std::size_t start, std::size_t) -> std::size_t
		{
			auto& log = *static_cast<ProgressLog*>(userData);
			++log.calls;
			log.startSum += start;
			return 1;
		};

		constexpr std::size_t Count = 8;
		constexpr std::size_t SumOfIndices = (Count - 1) * Count / 2;

		ProgressLog log;
		const TrackedTask tracked("ResumeProbe", func, &log);
		auto& task = *tracked;

		task.ReserveSubTasks(1);
		auto item = task.GenerateSubTask(0, Count, 0);

		for (std::size_t guard = 0; guard < Count * 2 && !item.HasFinished(); ++guard)
		{
			item.Run(task);
		}

		if (!item.HasFinished())
		{
			ls << "An item whose runnable advanced one index per call never finished; a stream would re-add it"
			   << " forever. It reported " << log.calls << " calls." << lferr;
		}

		if (log.calls != Count)
		{
			ls << "The runnable was called " << log.calls << " times for a range of " << Count
			   << " indices, so a partial return was not turned into one call per remaining index." << lferr;
		}

		if (log.startSum != SumOfIndices)
		{
			ls << "The runnable was handed starts summing to " << log.startSum << " instead of " << SumOfIndices
			   << ", so it was not resumed where it stopped: it was restarted, and every index before that point was"
			   << " done twice." << lferr;
		}

		if (item.current != Count)
		{
			ls << "A finished item left current at " << item.current << " rather than at its end." << lferr;
		}
	});

	AddTest("A split with nothing to do is refused rather than queued as a task that can never close",
			[this](TLogOut& ls)
	{
		auto& taskSys = Engine::Get().GetTaskSystem();
		const TaskSystem::TIndex workerStream = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerStream))
		{
			ls << "No worker stream to name." << lferr;
			return;
		}

		const TaskSystem::TIndex oneStream[] = {workerStream};

		std::atomic<unsigned> runs{0};
		auto countRun = [](void* userData, std::size_t startIndex, std::size_t endIndex) -> std::size_t
		{
			static_cast<std::atomic<unsigned>*>(userData)->fetch_add(1, std::memory_order_relaxed);
			return endIndex > startIndex ? endIndex - startIndex : 1;
		};

		const TaskID zeroItems =
				taskSys.ParallelFor("ZeroItemsSplit", countRun, &runs, 0, 4, oneStream, std::size(oneStream));
		const TaskID zeroJobs =
				taskSys.ParallelFor("ZeroJobsSplit", countRun, &runs, 8, 0, oneStream, std::size(oneStream));
		const TaskID noStreams = taskSys.ParallelFor("NoStreamsSplit", countRun, &runs, 8, 4, nullptr, 0);
		const TaskID negativeItems =
				taskSys.ParallelFor("NegativeItemsSplit", countRun, &runs, -8, 4, oneStream, std::size(oneStream));
		const TaskID negativeJobs =
				taskSys.ParallelFor("NegativeJobsSplit", countRun, &runs, 8, -4, oneStream, std::size(oneStream));
		const TaskID negativeStreams = taskSys.ParallelFor("NegativeStreamsSplit", countRun, &runs, 8, 4, -3);
		const TaskID successorNowhere = taskSys.ParallelFor("SuccessorNowhereSplit", countRun, &runs, 8, 4, oneStream,
															std::size(oneStream), 0, TaskID{7, 7});

		ls << "Refused as having nothing to do - zero items: " << (zeroItems.IsNull() ? "yes" : "no")
		   << ", zero sub-jobs: " << (zeroJobs.IsNull() ? "yes" : "no")
		   << ", no streams named: " << (noStreams.IsNull() ? "yes" : "no")
		   << ", negative items: " << (negativeItems.IsNull() ? "yes" : "no")
		   << ", negative sub-jobs: " << (negativeJobs.IsNull() ? "yes" : "no")
		   << ", negative stream count: " << (negativeStreams.IsNull() ? "yes" : "no")
		   << ". Refused for routing a successor nowhere: " << (successorNowhere.IsNull() ? "yes" : "no") << "." << lf;

		if (!zeroItems.IsNull() || !zeroJobs.IsNull() || !noStreams.IsNull() || !negativeItems.IsNull() ||
			!negativeJobs.IsNull() || !negativeStreams.IsNull())
		{
			ls << "A split with nothing to queue was accepted. That takes a registry record for a task whose join"
			   << " can never close, which is the state R29 calls fatal, made permanent. And a negative count is"
			   << " not a smaller split but an index conversion: the range arithmetic reaches GenerateSubTask as a"
			   << " size_t, so minus eight becomes a range of fourteen digits." << lferr;
		}

		if (successorNowhere.IsNull())
		{
			ls << "A split whose successor has no stream to run on was refused, which is R30's rule for a result"
			   << " with nowhere to go. If this engine ever gives a split a default destination, that default is a"
			   << " stream nobody chose." << lf;
		}
		else
		{
			ls << "A split was accepted with a successor and no stream for it, so its join will close and deliver"
			   << " nothing - the silent stop R30 reports out loud for every other task." << lferr;
		}

		if (runs.load() != 0)
		{
			ls << "Refused splits still ran " << runs.load() << " time(s)." << lferr;
		}
	});
}


} // namespace hbe
#endif //__UNIT_TEST__
