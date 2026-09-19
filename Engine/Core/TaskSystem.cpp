// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TaskSystem.h"

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
thread_local TaskSystem::TIndex StreamIndex = 0;
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
	, baseTaskThreadID(std::this_thread::get_id())
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
}

void TaskSystem::JoinAndClear() noexcept
{
	const bool isBaseThread = std::this_thread::get_id() == baseTaskThreadID;

	if (isBaseThread)
	{
		while (isRunning || mainThreadTaskQueue.HasPendingTasks())
		{
			ProcessMainThreadTasks();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
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

	streams.Clear();
}

void TaskSystem::Enqueue(const RangedTask& task) noexcept
{
	std::scoped_lock<std::mutex> lock(taskQueueMutex);
	taskQueue.Push(task);
}

void TaskSystem::Dequeue(std::optional<RangedTask>& outTask) noexcept
{
	std::scoped_lock<std::mutex> lock(taskQueueMutex);
	if (taskQueue.IsEmpty())
	{
		outTask.reset();
		return;
	}

	auto rangedTaskOpt = taskQueue.Top();
	if (!rangedTaskOpt.has_value())
	{
		outTask.reset();
		return;
	}

	const RangedTask& rangedTask = rangedTaskOpt.value();
	const unsigned int streamIndex = GetCurrentStreamIndex();

	auto& affinity = rangedTask.affinity;
	if (!affinity.Get(streamIndex))
	{
		affinity.Set(streamIndex);
		return;
	}

	outTask = rangedTask;
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

bool TaskSystem::Enqueue(const TIndex streamIndex, const RangedTask& task) noexcept
{
	if (!streams.IsValidIndex(streamIndex))
	{
		Assert(false, "Invalid stream index %d", streamIndex);

		return false;
	}

	auto& stream = streams[streamIndex];
	return stream.EnqueueFifo(task);
}

void TaskSystem::DispatchToMainThread(TMainThreadTask taskFunc, void* userData, uint8_t priority) noexcept
{
	mainThreadTaskQueue.Enqueue(taskFunc, userData, priority);
}

size_t TaskSystem::ProcessMainThreadTasks() noexcept
{
	return mainThreadTaskQueue.ProcessTasks();
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
	TIndex index = -1;

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
	Assert(IsBaseThread());
	FatalAssert(numHardwareThreads >= ENGINE_MIN_HARDWARE_THREADS,
				"Number of hardware threads are less than the minimum requirement");

	SetThreadName("Base");
	SetStreamIndex(-1);

	TIndex workerIndexStart = 0;

	auto log = Logger::Get(GetName());
	log.Out("# Creating TaskStreams ======================");

	streams.Swap(Array<TaskStream>(numHardwareThreads));

	// Pre-defined Engine Task Streams
	{
		auto index = GetBaseTaskStreamIndex();
		streams.Emplace(index, "Main", index);

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
}
} // namespace hbe

#ifdef __UNIT_TEST__
#include <memory>
#include "../Engine/Engine.h"
#include "Core/ResultContainer.h"
#include "OSAL/Intrinsic.h"
#include "Test/TestCollection.h"

namespace hbe
{

namespace
{

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

bool WaitForRunCount(const std::atomic<unsigned>& runs, unsigned expected,
					 std::chrono::milliseconds patience = std::chrono::seconds(5)) noexcept
{
	const auto deadline = std::chrono::steady_clock::now() + patience;
	while (runs.load(std::memory_order_relaxed) < expected && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return runs.load(std::memory_order_relaxed) >= expected;
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
		taskSys.Enqueue(task.GenerateSubTask(0, 0));
		task.BusyWait();

		if (!task.HasDone())
		{
			ls << "The task should be marked done after running." << lferr;
		}
	});

	AddTest("Bagel Problem", [this](TLogOut& ls)
	{
		constexpr std::size_t Count = 1000000;
		constexpr std::size_t NumSubtasks = 10;
		constexpr std::size_t Increment = Count / NumSubtasks;

		double result = 0;

		auto func = [](void* userData, std::size_t start, std::size_t end) -> std::size_t
		{
			double taskResult = 0;

			for (std::size_t i = start + 1; i <= end; ++i)
			{
				double value = 1.0 / static_cast<double>(i);
				value *= value;
				taskResult += value;
			}

			auto* totalSumPtr = static_cast<double*>(userData);
			double& totalSum = *totalSumPtr;
			totalSum += static_cast<float>(taskResult);

			return end - start;
		};

		const TrackedTask trackedTask("TestTask", func, &result);
		auto& task = *trackedTask;
		if (task.HasDone())
		{
			ls << "The task should not be marked done before running." << lferr;
		}

		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		for (std::size_t i = 0; i < Count; i += Increment)
		{
			taskSys.Enqueue(task.GenerateSubTask(i, i + Increment));
		}

		task.BusyWait();

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

		for (std::size_t i = 0; i < Count; i += Increment)
		{
			taskSys.Enqueue(task.GenerateSubTask(i, i + Increment));
		}

		task.BusyWait();

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

	AddTest("A task declares zero results until it says otherwise", [this](TLogOut& ls)
	{
		static_assert(std::numeric_limits<Task::TNumResults>::max() >= TaskStream::InitialResultCapacitySlots,
					  "A task must be able to declare as many results as a container can hold.");

		auto func = [](void*, std::size_t, std::size_t) -> std::size_t { return 1; };

		Task implicitTask;
		const TrackedTask trackedDeclared("ResultTask", func, nullptr);
		auto& declaredTask = *trackedDeclared;

		if (implicitTask.GetNumResults() != 0)
		{
			ls << "A default-constructed task declares " << implicitTask.GetNumResults()
			   << " results; zero is fire-and-forget and anything else makes a task that never delivers report"
			   << " completion it does not have." << lferr;
		}

		if (declaredTask.GetNumResults() != 0)
		{
			ls << "A task built with a runnable declares " << declaredTask.GetNumResults()
			   << " results by default. The default has to be the fire-and-forget answer, because a task built"
			   << " without thinking about results is one." << lferr;
		}

		constexpr Task::TNumResults declaredResults = 3;
		declaredTask.SetNumResults(declaredResults);
		if (declaredTask.GetNumResults() != declaredResults)
		{
			ls << "Declared " << declaredResults << " results and read back " << declaredTask.GetNumResults() << '.'
			   << lferr;
		}

		// A stream admits the work it dequeues, and what it dequeues is a subtask, so the declaration has to be
		// reachable from the subtask rather than only from the task it was written on.
		const auto subtask = declaredTask.GenerateSubTask(0, 1);
		if (subtask.declaredResults != declaredResults)
		{
			ls << "A subtask of a task declaring " << declaredResults << " results sees " << subtask.declaredResults
			   << " on the subtask it generated." << lferr;
		}

		// The count has to be able to say a whole container, or the initial capacity is a ceiling no caller can
		// state and admission would be refusing a number that was never expressible.
		declaredTask.SetNumResults(static_cast<Task::TNumResults>(TaskStream::InitialResultCapacitySlots));
		if (declaredTask.GetNumResults() != TaskStream::InitialResultCapacitySlots)
		{
			ls << "A task could not declare the " << TaskStream::InitialResultCapacitySlots << " results a fresh"
			   << " container holds; it read back " << declaredTask.GetNumResults() << '.' << lferr;
		}

		ls << "Declared " << TaskStream::InitialResultCapacitySlots << " results and read back "
		   << declaredTask.GetNumResults() << '.' << lf;
	});

	AddTest("Every stream is built with two result containers of 1024 slots", [this](TLogOut& ls)
	{
		// The decided figures are written out here instead of being read back from the constants that produced
		// them. A test comparing a value against the same constant it came from cannot disagree with it, which
		// makes that comparison decorative rather than wrong - measured, after changing the initial capacity to 512
		// and watching every assertion below stay green. The static_asserts are what tie these numbers to the
		// engine's, so changing a decision means changing it in two places deliberately.
		constexpr std::size_t decidedContainers = 2;
		constexpr std::size_t decidedCapacitySlots = 1024;
		constexpr std::size_t decidedGrowBySlots = 1024;

		static_assert(TaskStream::NumResultContainers == decidedContainers,
					  "A stream has two result containers, so filling and consuming do not have to overlap.");
		static_assert(TaskStream::InitialResultCapacitySlots == decidedCapacitySlots,
					  "A result container starts at 1024 slots, which is 131,072 bytes and one eighth of a megabyte.");
		static_assert(TaskStream::DefaultGrowBySlots == decidedGrowBySlots,
					  "A stream grows its result containers by 1024 slots, one number for every stream.");

		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		if (TaskStream::DefaultGrowBySlots != TaskStream::InitialResultCapacitySlots)
		{
			ls << "A stream grows its result containers by " << TaskStream::DefaultGrowBySlots << " slots while a"
			   << " container starts at " << TaskStream::InitialResultCapacitySlots << "; the decided figure is one"
			   << " number for both, which is what makes a container one eighth of a megabyte." << lferr;
		}

		for (int index = 0; taskSys.HasStream(index); ++index)
		{
			const auto& stream = taskSys.GetStream(index);

			if (stream.GetResultGrowBy() != decidedGrowBySlots)
			{
				ls << stream.GetName().c_str() << " grows its result containers by " << stream.GetResultGrowBy()
				   << " slots, not " << decidedGrowBySlots << ", so this stream was built to a ceiling nobody stated."
				   << lferr;
			}

			for (std::size_t containerIndex = 0; containerIndex < decidedContainers; ++containerIndex)
			{
				const auto& container = stream.GetResultContainer(containerIndex);
				if (container.GetCapacity() != decidedCapacitySlots)
				{
					ls << stream.GetName().c_str() << " result container " << containerIndex << " holds "
					   << container.GetCapacity() << " slots, not " << decidedCapacitySlots << '.' << lferr;
				}
			}

			ls << stream.GetName().c_str() << ": " << decidedContainers << " containers of " << decidedCapacitySlots
			   << " slots, growing by " << stream.GetResultGrowBy() << '.' << lf;
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
		for (unsigned index = 0; index < fifoTasks; ++index)
		{
			(void) stream.EnqueueFifo(laneTask.GenerateSubTask(index, index + 1));
		}

		(void) stream.EnqueuePriority(laneTask.GenerateSubTask(fifoTasks, fifoTasks + 1));

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
		stream.ConfigureBudget(allowance);

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
		(void) taskSys.Enqueue(workerIndex, busyTask.GenerateSubTask(0, 1));
		busyTask.Wait(1);

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

		stream.ConfigureBudget(std::chrono::duration<double>{});

		if (!stream.MayTakeNewWork())
		{
			ls << "Restoring the unlimited allowance did not restore willingness to take work." << lferr;
		}
	});

	AddTest("Every declaration is admitted until a stream is given a ceiling", [this](TLogOut& ls)
	{
		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		const auto workerIndex = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerIndex))
		{
			ls << "No worker stream at index " << workerIndex << ", so a ceiling could not be observed." << lferr;
			return;
		}

		auto& stream = taskSys.GetStream(workerIndex);

		// The decided default is written out rather than read back from DefaultMaxResultCapacitySlots, which is
		// the constant that produced it.
		constexpr std::size_t decidedDefaultCeilingSlots = 0;
		if (stream.GetResultMaxCapacity() != decidedDefaultCeilingSlots)
		{
			ls << stream.GetName().c_str() << " starts with a ceiling of " << stream.GetResultMaxCapacity()
			   << " slots; R21 decides that an unconfigured stream caps nothing." << lferr;
		}

		constexpr Task::TNumResults beyondEveryContainer = 1U << 20U;
		if (!stream.CanAdmitResults(beyondEveryContainer))
		{
			ls << "A stream with no ceiling refused a declaration of " << beyondEveryContainer
			   << ", so the guard is not dormant as R21 decides it should be." << lferr;
		}

		static std::atomic<unsigned> runs{0};
		runs.store(0, std::memory_order_relaxed);
		auto countRun = [](void*, std::size_t, std::size_t) -> std::size_t
		{
			runs.fetch_add(1, std::memory_order_relaxed);
			return 1;
		};

		const TrackedTask trackedBig("BigDeclaration", countRun, nullptr);
		auto& bigDeclaration = *trackedBig;
		bigDeclaration.SetNumResults(beyondEveryContainer);

		if (!taskSys.Enqueue(workerIndex, bigDeclaration.GenerateSubTask(0, 1)))
		{
			ls << "Enqueue refused a task declaring " << beyondEveryContainer << " results on a stream with no"
			   << " ceiling, so work is being dropped by a guard that should never fire." << lferr;
			return;
		}

		if (!WaitForRunCount(runs, 1))
		{
			ls << "A task admitted under an unlimited ceiling never ran." << lferr;
		}

		ls << stream.GetName().c_str() << " has a ceiling of " << stream.GetResultMaxCapacity()
		   << " and admitted a declaration of " << beyondEveryContainer << '.' << lf;
	});

	AddTest("A task declaring more than the ceiling is refused at dispatch", [this](TLogOut& ls)
	{
		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		const auto workerIndex = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerIndex))
		{
			ls << "No worker stream at index " << workerIndex << ", so a ceiling could not be applied." << lferr;
			return;
		}

		auto& stream = taskSys.GetStream(workerIndex);

		constexpr Task::TNumResults ceiling = 2;
		stream.SetResultMaxCapacity(ceiling);

		static std::atomic<unsigned> admittedRuns{0};
		static std::atomic<unsigned> refusedRuns{0};
		admittedRuns.store(0, std::memory_order_relaxed);
		refusedRuns.store(0, std::memory_order_relaxed);

		auto countAdmitted = [](void*, std::size_t, std::size_t) -> std::size_t
		{
			admittedRuns.fetch_add(1, std::memory_order_relaxed);
			return 1;
		};
		auto countRefused = [](void*, std::size_t, std::size_t) -> std::size_t
		{
			refusedRuns.fetch_add(1, std::memory_order_relaxed);
			return 1;
		};

		// Both boundaries: a declaration exactly at the ceiling is admissible, and one past it is not.
		const TrackedTask trackedAtCeiling("AtCeiling", countAdmitted, nullptr);
		auto& atCeiling = *trackedAtCeiling;
		atCeiling.SetNumResults(ceiling);
		const bool atCeilingAdmitted = taskSys.Enqueue(workerIndex, atCeiling.GenerateSubTask(0, 1));

		const TrackedTask trackedPastCeiling("PastCeiling", countRefused, nullptr);
		auto& pastCeiling = *trackedPastCeiling;
		pastCeiling.SetNumResults(ceiling + 1);
		const bool pastCeilingAdmitted = taskSys.Enqueue(workerIndex, pastCeiling.GenerateSubTask(0, 1));

		const bool ranAtCeiling = WaitForRunCount(admittedRuns, 1);
		const bool ranPastCeiling = WaitForRunCount(refusedRuns, 1, std::chrono::milliseconds(200));

		stream.SetResultMaxCapacity(TaskStream::DefaultMaxResultCapacitySlots);

		ls << "Ceiling " << ceiling << ": a declaration of " << ceiling << " was "
		   << (atCeilingAdmitted ? "admitted" : "refused") << " and a declaration of " << ceiling + 1 << " was "
		   << (pastCeilingAdmitted ? "admitted" : "refused") << '.' << lf;

		if (!atCeilingAdmitted)
		{
			ls << "A task declaring exactly the ceiling was refused, so the ceiling excludes the count it is set"
			   << " to instead of the counts above it." << lferr;
		}
		if (!ranAtCeiling)
		{
			ls << "A task the ceiling admits never ran." << lferr;
		}
		if (pastCeilingAdmitted)
		{
			ls << "A task declaring " << ceiling + 1 << " results was admitted on a stream capped at " << ceiling
			   << ", so the guard does not refuse what it exists to refuse." << lferr;
		}
		if (ranPastCeiling)
		{
			ls << "A refused task ran anyway, so the refusal did not keep it out of the lane." << lferr;
		}
	});

	AddTest("Fire-and-forget work is admitted under any ceiling", [this](TLogOut& ls)
	{
		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		const auto workerIndex = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerIndex))
		{
			ls << "No worker stream at index " << workerIndex << ", so a ceiling could not be applied." << lferr;
			return;
		}

		auto& stream = taskSys.GetStream(workerIndex);

		constexpr Task::TNumResults ceiling = 1;
		stream.SetResultMaxCapacity(ceiling);

		static std::atomic<unsigned> runs{0};
		runs.store(0, std::memory_order_relaxed);
		auto countRun = [](void*, std::size_t, std::size_t) -> std::size_t
		{
			runs.fetch_add(1, std::memory_order_relaxed);
			return 1;
		};

		// Zero results declared is fire-and-forget, the commonest task on a stream, and the tightest ceiling in
		// the engine still has to run it.
		const TrackedTask trackedNoResults("FireAndForget", countRun, nullptr);
		auto& noResults = *trackedNoResults;

		const bool admitted = stream.EnqueueFifo(noResults.GenerateSubTask(0, 1));
		const bool ran = WaitForRunCount(runs, 1);

		stream.SetResultMaxCapacity(TaskStream::DefaultMaxResultCapacitySlots);

		ls << "Ceiling " << ceiling << ": a task declaring " << noResults.GetNumResults() << " results was "
		   << (admitted ? "admitted" : "refused") << " and " << (ran ? "ran" : "never ran") << '.' << lf;

		if (!admitted || !ran)
		{
			ls << "A task declaring no results at all was not run under a ceiling of " << ceiling
			   << ", so the ceiling is capping tasks rather than results." << lferr;
		}
	});

	AddTest("The priority lane refuses an unreachable declaration too", [this](TLogOut& ls)
	{
		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		const auto workerIndex = TaskSystem::GetIOTaskStreamIndex() + 1;
		if (!taskSys.HasStream(workerIndex))
		{
			ls << "No worker stream at index " << workerIndex << ", so a ceiling could not be applied." << lferr;
			return;
		}

		auto& stream = taskSys.GetStream(workerIndex);

		constexpr Task::TNumResults ceiling = 2;
		stream.SetResultMaxCapacity(ceiling);

		static std::atomic<unsigned> refusedRuns{0};
		refusedRuns.store(0, std::memory_order_relaxed);
		auto countRun = [](void*, std::size_t, std::size_t) -> std::size_t
		{
			refusedRuns.fetch_add(1, std::memory_order_relaxed);
			return 1;
		};

		const TrackedTask trackedPriorityPast("PriorityPastCeiling", countRun, nullptr);
		auto& pastCeiling = *trackedPriorityPast;
		pastCeiling.SetNumResults(ceiling + 1);
		const bool admitted = stream.EnqueuePriority(pastCeiling.GenerateSubTask(0, 1));

		const bool ran = WaitForRunCount(refusedRuns, 1, std::chrono::milliseconds(200));

		stream.SetResultMaxCapacity(TaskStream::DefaultMaxResultCapacitySlots);

		ls << "Priority lane, ceiling " << ceiling << ": a declaration of " << ceiling + 1 << " was "
		   << (admitted ? "admitted" : "refused") << " and the task " << (ran ? "ran" : "did not run") << '.' << lf;

		if (admitted)
		{
			ls << "The priority lane accepted a declaration the FIFO lane refuses, so the ceiling guards one lane"
			   << " of two." << lferr;
		}
		if (ran)
		{
			ls << "A task refused on the priority lane ran anyway." << lferr;
		}
	});
}

} // namespace hbe
#endif //__UNIT_TEST__
