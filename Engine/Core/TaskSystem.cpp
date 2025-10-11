// Created by mooming.go@gmail.com, 2022

#include "TaskSystem.h"

#include <exception>
#include <future>
#include <thread>
#include "Config/ConfigParam.h"
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
			static TConfigParam<uint8_t> CPLogLevel("Log.TaskSystem", "The TaskSystem Log Level",
													static_cast<uint8_t>(ELogLevel::Warning));

			return level > static_cast<ELogLevel>(CPLogLevel.Get());
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

		outTask = taskQueue.top();
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
#include "ScopedTime.h"
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
			auto& engine = Engine::Get();
			auto& taskSys = engine.GetTaskSystem();

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

			taskSys.Enqueue(task.GenerateSubTask(0, 0));
			task.BusyWait();

			if (!task.HasDone())
			{
				ls << "The task should be marked done after running." << lferr;
			}
		});

#if 0
		AddTest("Resident Task(sync)", [](TLogOut& ls)
		{
			auto& engine = Engine::Get();
			auto& taskSys = engine.GetTaskSystem();

			int count = 0;

			auto func = [&count](auto start, auto end)
			{
				if ((count % 10000) == 0)
				{
					auto log = Logger::Get("Resident Task");
					log.Out([count](auto& ls) { ls << "Resident Frame Count = " << count; });
				}

				++count;
			};

			StaticString taskName("DummyCount");
			const auto streamIndex = taskSys.GetWorkerIndexStart();

			auto handle = taskSys.RegisterTask(streamIndex, taskName, func);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		});

		AddTest("Resident Task(async)", [](auto& ls)
		{
			auto& engine = Engine::Get();
			auto& taskSys = engine.GetTaskSystem();

			auto count = std::make_shared<uint32_t>(0);

			auto func = [count](auto start, auto end)
			{
				uint32_t value = *count;
				if ((value % 10000) == 0)
				{
					auto log = Logger::Get("Resident Task");
					log.Out([value](auto& ls) { ls << "Resident Frame Count = " << value; });
				}

				++(*count);
			};

			StaticString taskName("DummyCount");
			const auto streamIndex = taskSys.GetWorkerIndexStart();

			auto handle = taskSys.RegisterTask(streamIndex, taskName, func);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		});

		AddTest("Sum", [this](auto& ls)
		{
			std::atomic<uint64_t> sum = 0;

			auto func = [&sum](auto start, auto end)
			{
				auto log = Logger::Get("Sum");
				log.Out([start, end](auto& ls) { ls << "Start: Sum Range[" << (start + 1) << ", " << end << ']'; });

				uint64_t result = 0;
				for (uint64_t i = start + 1; i <= end; ++i)
				{
					result += i;
				}

				sum += result;

				log.Out([start, end, result](auto& ls)
				{ ls << "Sum Range[" << (start + 1) << ", " << end << "] = " << result; });
			};

			auto& engine = Engine::Get();
			auto& taskSys = engine.GetTaskSystem();
			auto handle = taskSys.RequestTask("SummationTask", 10000000, func);
			if (!handle.IsValid())
			{
				ls << "Handle should not be invalid." << lferr;
				return;
			}

			auto task = handle.GetTask();
			if (task == nullptr)
			{
				ls << "DispatchTask failed due to null task." << lferr;
				return;
			}

			handle.BusyWait();

			ls << "Sum = " << sum << ", done = " << task->NumDone() << "/" << task->NumStreams() << lf;

			if (sum != 50000005000000L)
			{
				ls << "Incorrect summation result = " << sum << lferr;
			}
		});

		auto countPrimeNumbers = [](auto& ls, auto& lf, auto& lferr, int numWorkers)
		{
			auto IsPrimeNumbrer = [](uint64_t value) -> bool
			{
				for (uint64_t i = 2; i < value; ++i)
				{
					if ((value % i) == 0)
					{
						return false;
					}
				}

				return true;
			};

			std::atomic<uint64_t> count = 0;
			std::atomic<int> numThreads = 0;

			auto func = [&count, &numThreads, IsPrimeNumbrer](auto start, auto end)
			{
				++numThreads;

				auto log = Logger::Get("PNCounter");
				log.Out([&](auto& ls) { ls << "Start: Range[" << (start + 1) << ", " << end << ']'; });

				uint64_t result = 0;

				for (uint64_t i = start + 1; i <= end; ++i)
				{
					if (IsPrimeNumbrer(i))
					{
						++result;
					}
				}

				count += result;

				log.Out([&](auto& ls) { ls << "Range[" << (start + 1) << ", " << end << "] = " << result; });
			};

			constexpr int upperBound = 100000;
			constexpr int solution = 9592;

			auto& engine = Engine::Get();
			auto& taskSys = engine.GetTaskSystem();
			TaskHandle handle;

			time::TDuration duration;
			{
				time::ScopedTime timer(duration);

				handle = taskSys.RequestTask("PNCounter", upperBound, func, numWorkers);

				if (!handle.IsValid())
				{
					ls << "Handle should not be invalid." << lferr;
					return;
				}

				auto taskPtr = handle.GetTask();
				if (taskPtr == nullptr)
				{
					ls << "Task is null." << lferr;
					debugBreak();
				}

				handle.BusyWait();
			}

			count = count - 1;

			if (numThreads != numWorkers)
			{
				ls << "Number of threads must be " << numWorkers << ", but " << numThreads << lferr;
			}

			auto taskPtr = handle.GetTask();
			if (taskPtr == nullptr)
			{
				ls << "Task is null." << lferr;
				debugBreak();
			}

			auto& task = *(taskPtr);
			ls << "# of Prime Numbers = " << count << ", done = " << task.NumDone() << "/" << task.NumStreams()
			   << ", time = " << time::ToFloat(duration) << " seconds" << lf;

			if (count != solution)
			{
				ls << "Number of prime numbers below " << upperBound << " should be " << solution << " but " << count
				   << " found!" << lferr;
			}
		};

		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();
		auto numWorkers = taskSys.GetNumWorkers();
		for (int i = 1; i <= numWorkers; ++i)
		{
			InlineStringBuilder<> testName;
			testName << "CountPrimeNumbers with " << i << " workers";

			AddTest(testName.c_str(), [this, countPrimeNumbers, i](auto& ls) { countPrimeNumbers(ls, lf, lferr, i); });
		}
#endif
	}

} // namespace hbe
#endif //__UNIT_TEST__
