// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TaskSystem.h"

#include <exception>
#include <future>
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

	TaskSystem::TIndex TaskSystem::getNumHardwareThreads() noexcept
	{
		const auto hardwareConcurrency = std::thread::hardware_concurrency();
		const auto numAvaibleHardwareThreads = static_cast<TIndex>(hardwareConcurrency);

		return numAvaibleHardwareThreads;
	}

	void TaskSystem::setThreadName(StaticString name) noexcept
	{
		ThreadName = name;
	}

	void TaskSystem::setStreamIndex(TIndex index) noexcept
	{
		StreamIndex = index;
	}

	StaticString TaskSystem::getCurrentStreamName() noexcept
	{
		return ThreadName;
	}

	StaticString TaskSystem::getCurrentThreadName() noexcept
	{
		return ThreadName;
	}

	TaskSystem::TIndex TaskSystem::getCurrentStreamIndex() noexcept
	{
		return StreamIndex;
	}

	bool TaskSystem::isBaseThread() noexcept
	{
		return StreamIndex == BaseStreamIndex;
	}

	bool TaskSystem::isIOThread() noexcept
	{
		return StreamIndex == IOStreamIndex;
	}

	TaskSystem::TaskSystem() noexcept
		: isRunning(false)
		, name("TaskSystem")
		, numHardwareThreads(getNumHardwareThreads())
		, baseTaskThreadID(std::this_thread::get_id())
	{
		fatalAssert(numHardwareThreads > 0, "It should have at least one hardware thread.");
	}

	TaskSystem::~TaskSystem() noexcept
	{
		joinAndClear();
	}

	void TaskSystem::initialize() noexcept
	{
		auto& logger = Logger::get();
		auto logFilter = [](auto level)
		{
			static TAtomicConfigParam<uint8_t> logLevel("Log.TaskSystem", "The TaskSystem Log Level",
													static_cast<uint8_t>(ELogLevel::Warning));

			return level > static_cast<ELogLevel>(logLevel.get());
		};

		logger.setFilter(getName(), logFilter);

		auto log = Logger::get(getName());
		log.out([this](auto& ls) { ls << "Hardware Concurrency = " << numHardwareThreads; });

		buildStreams();
	}

	void TaskSystem::requestShutDown() noexcept
	{
		isRunning = false;
	}

	void TaskSystem::joinAndClear() noexcept
	{
		const bool isBaseThread = std::this_thread::get_id() == baseTaskThreadID;

		if (isBaseThread)
		{
			while (isRunning || mainThreadTaskQueue.HasPendingTasks())
			{
				processMainThreadTasks();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		for (auto& stream : streams)
		{
			auto& thread = stream.getThread();
			if (unlikely(!thread.joinable()))
			{
				continue;
			}

			thread.join();
		}

		streams.clear();
	}

	void TaskSystem::enqueue(const RangedTask& task) noexcept
	{
		std::scoped_lock<std::mutex> lock(taskQueueMutex);
		taskQueue.push(task);
	}

	void TaskSystem::dequeue(std::optional<RangedTask>& outTask) noexcept
	{
		std::scoped_lock<std::mutex> lock(taskQueueMutex);
		if (taskQueue.IsEmpty())
		{
			outTask.reset();
			return;
		}

		auto rangedTaskOpt = taskQueue.top();
		if (!rangedTaskOpt.has_value())
		{
			outTask.reset();
			return;
		}

		const RangedTask& rangedTask = rangedTaskOpt.value();
		const unsigned int streamIndex = getCurrentStreamIndex();

		auto& affinity = rangedTask.affinity;
		if (!affinity.get(streamIndex))
		{
			affinity.set(streamIndex);
			return;
		}

		outTask = rangedTask;
		(void)taskQueue.pop();
	}

	void TaskSystem::enqueue(const TIndex streamIndex, const RangedTask& task) noexcept
	{
		if (!streams.isValidIndex(streamIndex))
		{
			Assert(false, "Invalid stream index %d", streamIndex);

			return;
		}

		auto& stream = streams[streamIndex];
		stream.enqueue(task);
	}

	void TaskSystem::dispatchToMainThread(TMainThreadTask taskFunc, void* userData, uint8_t priority) noexcept
	{
		mainThreadTaskQueue.enqueue(taskFunc, userData, priority);
	}

	size_t TaskSystem::processMainThreadTasks() noexcept
	{
		return mainThreadTaskQueue.processTasks();
	}

	StaticString TaskSystem::getStreamName(int index) const noexcept
	{
		if (unlikely(!streams.isValidIndex(index)))
		{
			static StaticString unknown("Unknown");
			return unknown;
		}

		return streams[index].getName();
	}

	TaskSystem::TIndex TaskSystem::getStreamIndex(TThreadID id) const noexcept
	{
		TIndex index = -1;

		auto size = streams.Size();
		for (decltype(size) i = 0; i < size; ++i)
		{
			if (auto& stream = streams[i]; stream.getThreadID() != id)
			{
				continue;
			}

			index = i;
			break;
		}

		return index;
	}

	TaskStream& TaskSystem::getStream(int index) noexcept
	{
		if (index < 0 || index >= streams.Size())
		{
			return streams[0];
		}

		return streams[index];
	}

	void TaskSystem::buildStreams()
	{
		Assert(isBaseThread());
		fatalAssert(numHardwareThreads >= ENGINE_MIN_HARDWARE_THREADS,
			"Number of hardware threads are less than the minimum requirement");

		setThreadName("Base");
		setStreamIndex(-1);

		TIndex workerIndexStart = 0;

		auto log = Logger::get(getName());
		log.out("# Creating TaskStreams ======================");

		streams.Swap(Array<TaskStream>(numHardwareThreads));

		// Pre-defined Engine Task Streams
		{
			auto index = getBaseTaskStreamIndex();
			streams.emplace(index, "Main", index);

			index = getIOTaskStreamIndex();
			streams.emplace(index, "IO", index);
		}

		workerIndexStart = getIOTaskStreamIndex() + 1;

		TIndex numWorkers = 0;

		InlineStringBuilder<64> streamName;
		for (TIndex i = workerIndexStart; i < numHardwareThreads; ++i)
		{
			++numWorkers;
			streamName << "Worker" << numWorkers;

			streams.emplace(i, streamName.c_str(), i);
			streamName.clear();
		}

		log.out("# Starting TaskStreams ======================");

		isRunning = true;

		for (auto& stream : streams)
		{
			stream.start(*this);
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

void TaskSystemTest::prepare()
{
	addTest("Empty Task", [this](TLogOut& ls)
	{
		Task task;
		if (task.HasDone())
		{
			ls << "Dummy task should not be set to done before it's enqueued." << lferr;
		}
	});

	addTest("Task of size 0", [this](TLogOut& ls)
	{
		auto func = [](void*, std::size_t start, std::size_t end) -> std::size_t
		{
			auto log = Logger::get("Size 0 Task");
			log.out([&](auto& ls) { ls << "Range[" << (start + 1) << ", " << end << ')'; });

			return 1;
		};

		Task task("TestTask", func, nullptr);
		if (task.HasDone())
		{
			ls << "The task should not be marked done before running." << lferr;
		}

		auto& engine = Engine::get();
		auto& taskSys = engine.getTaskSystem();
		taskSys.enqueue(task.generateSubTask(0, 0));
		task.busyWait();

		if (!task.HasDone())
		{
			ls << "The task should be marked done after running." << lferr;
		}
	});

	addTest("Bagel Problem", [this](TLogOut& ls)
	{
		constexpr std::size_t Count = 1000000;
		constexpr std::size_t NumSubtasks = 10;
		constexpr std::size_t Increment = Count / NumSubtasks;

		double result = 0;

		auto func = [](void* userData, std::size_t start, std::size_t end) -> std::size_t
		{
			double taskResult = 0;

			for(std::size_t i = start + 1; i <= end; ++i)
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

		Task task("TestTask", func, &result);
		if (task.HasDone())
		{
			ls << "The task should not be marked done before running." << lferr;
		}

		auto& engine = Engine::get();
		auto& taskSys = engine.getTaskSystem();

		for (std::size_t i = 0; i < Count; i += Increment)
		{
			taskSys.enqueue(task.generateSubTask(i , i + Increment));
		}

		task.busyWait();

		if (!task.HasDone())
		{
			ls << "The task should be marked done after running." << lferr;
		}

		constexpr double EulerAnswer = Pi * Pi/ 6.0;
		ls << "Test Result = " << result << ", Pi/6 = " << EulerAnswer << lf;

		const double error = std::round(result - EulerAnswer);
		if (error > Epsilon)
		{
			ls << "The error exceeds limit. Error = " << error << lferr;
		}
	});

	addTest("Bagel Problem (Incremental Task)", [this](TLogOut& ls)
	{
		constexpr std::size_t Count = 1000000;
		constexpr std::size_t NumSubtasks = 5;
		constexpr std::size_t Increment = Count / NumSubtasks;

		double result = 0;

		auto func = [](void* userData, std::size_t start, std::size_t end) -> std::size_t
		{
			double taskResult = 0;

			auto incEnd = std::min(end, start + (Increment / 7));
			for(std::size_t i = start + 1; i <= incEnd; ++i)
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

		Task task("TestTask", func, &result);
		if (task.HasDone())
		{
			ls << "The task should not be marked done before running." << lferr;
		}

		auto& engine = Engine::get();
		auto& taskSys = engine.getTaskSystem();

		for (std::size_t i = 0; i < Count; i += Increment)
		{
			taskSys.enqueue(task.generateSubTask(i , i + Increment));
		}

		task.busyWait();

		if (!task.HasDone())
		{
			ls << "The task should be marked done after running." << lferr;
		}

		constexpr double EulerAnswer = Pi * Pi/ 6.0;
		ls << "Test Result = " << result << ", Pi/6 = " << EulerAnswer << lf;

		const double error = std::round(result - EulerAnswer);
		if (error > Epsilon)
		{
			ls << "The error exceeds limit. Error = " << error << lferr;
		}
	});
}

} // namespace hbe
#endif //__UNIT_TEST__
