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
}

void TaskSystem::JoinAndClear() noexcept
{
	const bool isEngineLoopThread = std::this_thread::get_id() == engineLoopThreadID;

	if (isEngineLoopThread)
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

void TaskSystem::Enqueue(const TIndex streamIndex, const RangedTask& task) noexcept
{
	if (!streams.IsValidIndex(streamIndex))
	{
		Assert(false, "Invalid stream index %d", streamIndex);
		return;
	}

	streams[streamIndex].EnqueueFifo(task);
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
		taskSys.Enqueue(workerIndex, busyTask.GenerateSubTask(0, 1));
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
}

} // namespace hbe
#endif //__UNIT_TEST__
