// Created by mooming.go@gmail.com, 2022

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

	TaskSystem::TIndex TaskSystem::GetNumHardwareThreads()
	{
		const auto hardwareConcurrency = std::thread::hardware_concurrency();
		const auto numAvaibleHardwareThreads = static_cast<TIndex>(hardwareConcurrency);

		return numAvaibleHardwareThreads;
	}

	void TaskSystem::SetThreadName(StaticString name)
	{
		ThreadName = name;
	}

	void TaskSystem::SetStreamIndex(TIndex index)
	{
		StreamIndex = index;
	}

	StaticString TaskSystem::GetCurrentStreamName()
	{
		return ThreadName;
	}

	StaticString TaskSystem::GetCurrentThreadName()
	{
		return ThreadName;
	}

	TaskSystem::TIndex TaskSystem::GetCurrentStreamIndex()
	{
		return StreamIndex;
	}

	bool TaskSystem::IsMainThread()
	{
		return StreamIndex == MainStreamIndex;
	}

	bool TaskSystem::IsIOThread()
	{
		return StreamIndex == IOStreamIndex;
	}

	TaskSystem::TaskSystem()
		: isRunning(false)
		, name("TaskSystem")
		, numHardwareThreads(GetNumHardwareThreads())
		, mainTaskThreadID(std::this_thread::get_id())
	{
		FatalAssert(numHardwareThreads > 0, "It should have at least one hardware thread.");
	}

	TaskSystem::~TaskSystem()
	{
		JoinAndClear();
	}

	void TaskSystem::Initialize()
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

		BuildStreams();
	}

	void TaskSystem::RequestShutDown()
	{
		isRunning = false;
	}

	void TaskSystem::JoinAndClear()
	{
		for (auto& stream : streams)
		{
			auto& thread = stream.GetThread();
			if (unlikely(!thread.joinable()))
			{
				continue;
			}

			try
			{
				if (thread.joinable() && thread.get_id() != std::this_thread::get_id())
				{
					thread.join();
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << ", thread = " << stream.GetName() << ", loop count = " << stream.GetLoopCount()
					<< ", Joinable = " << thread.joinable() << std::endl;
				std::flush(std::cerr);
				debugBreak();
			}
		}

		try
		{
			streams.Clear();
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << std::endl;
			std::flush(std::cerr);
			debugBreak();
		}
	}

	void TaskSystem::Enqueue(const RangedTask& task)
	{
		std::scoped_lock<std::mutex> lock(taskQueueMutex);
		taskQueue.push(task);
	}

	void TaskSystem::Dequeue(std::optional<RangedTask>& outTask)
	{
		std::scoped_lock<std::mutex> lock(taskQueueMutex);
		if (taskQueue.empty())
		{
			outTask.reset();
			return;
		}

		const RangedTask& rangedTask = taskQueue.top();
		const unsigned int streamIndex = GetCurrentStreamIndex();

		auto& affinity = rangedTask.affinity;
		if (!affinity.Get(streamIndex))
		{
			affinity.Set(streamIndex);
			return;
		}

		outTask = rangedTask;
		taskQueue.pop();
	}

	void TaskSystem::Enqueue(const TIndex streamIndex, const RangedTask& task)
	{
		if (!streams.IsValidIndex(streamIndex))
		{
			Assert(false, "Invalid stream index %d", streamIndex);

			return;
		}

		auto& stream = streams[streamIndex];
		stream.Enqueue(task);
	}

	StaticString TaskSystem::GetStreamName(int index) const
	{
		if (unlikely(!streams.IsValidIndex(index)))
		{
			static StaticString unknown("Unknown");
			return unknown;
		}

		return streams[index].GetName();
	}

	TaskSystem::TIndex TaskSystem::GetStreamIndex(ThreadID id) const
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

	TaskStream& TaskSystem::GetStream(int index)
	{
		if (index < 0 || index >= streams.Size())
		{
			return streams[0];
		}

		return streams[index];
	}

	void TaskSystem::BuildStreams()
	{
		Assert(IsMainThread());
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
			auto index = GetMainTaskStreamIndex();
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
#include "OSAL/Intrinsic.h"
#include "Test/TestCollection.h"

namespace hbe
{

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

			Task task("TestTask", func, nullptr);
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

				for(std::size_t i = start + 1; i <= end; ++i)
				{
					double value = 1.0 / static_cast<double>(i);
					value *= value;
					taskResult += value;
				}

				auto* totalSumPtr = static_cast<double*>(userData);
				double& totalSum = *totalSumPtr;
				totalSum += static_cast<float>(taskResult);

				auto log = Logger::Get("Bagel Problem");
				log.Out([=](auto& ls) { ls << "Num[" << (start + 1) << ", " << end << "], result = " << taskResult; });

				return end - start;
			};

			Task task("TestTask", func, &result);
			if (task.HasDone())
			{
				ls << "The task should not be marked done before running." << lferr;
			}

			auto& engine = Engine::Get();
			auto& taskSys = engine.GetTaskSystem();

			for (std::size_t i = 0; i < Count; i += Increment)
			{
				taskSys.Enqueue(task.GenerateSubTask(i , i + Increment));
			}

			task.BusyWait();

			if (!task.HasDone())
			{
				ls << "The task should be marked done after running." << lferr;
			}

			constexpr double EulerAnswer = Pi * Pi/ 6.0;
			ls << "Test Result = " << result << ", Pi/6 = " << EulerAnswer << lf;

			const double error = std::abs(result - EulerAnswer);
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
				for(std::size_t i = start + 1; i <= incEnd; ++i)
				{
					double value = 1.0 / static_cast<double>(i);
					value *= value;
					taskResult += value;
				}

				auto* totalSumPtr = static_cast<double*>(userData);
				double& totalSum = *totalSumPtr;
				totalSum += static_cast<float>(taskResult);

				auto log = Logger::Get("Bagel Problem (Incremental Task)");
				log.Out([=](auto& ls) { ls << "Num[" << (start + 1) << ", " << incEnd << "], result = " << taskResult; });

				return incEnd - start;
			};

			Task task("TestTask", func, &result);
			if (task.HasDone())
			{
				ls << "The task should not be marked done before running." << lferr;
			}

			auto& engine = Engine::Get();
			auto& taskSys = engine.GetTaskSystem();

			for (std::size_t i = 0; i < Count; i += Increment)
			{
				taskSys.Enqueue(task.GenerateSubTask(i , i + Increment));
			}

			task.BusyWait();

			if (!task.HasDone())
			{
				ls << "The task should be marked done after running." << lferr;
			}

			constexpr double EulerAnswer = Pi * Pi/ 6.0;
			ls << "Test Result = " << result << ", Pi/6 = " << EulerAnswer << lf;

			const double error = std::abs(result - EulerAnswer);
			if (error > Epsilon)
			{
				ls << "The error exceeds limit. Error = " << error << lferr;
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
