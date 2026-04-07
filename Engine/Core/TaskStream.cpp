// Created by mooming.go@gmail.com, 2022

#include "TaskStream.h"

#include <thread>

#include "Config/ConfigParam.h"
#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "OSAL/OSThread.h"
#include "ScopedTime.h"
#include "TaskSystem.h"

namespace hbe
{

	TaskStream::TaskQueueItem::TaskQueueItem(uint8_t priority, const RangedTask& task)
		: priority(priority)
		, task(task)
		, duration(0)
	{}

	bool TaskStream::TaskQueueItem::operator<(const TaskQueueItem& other) const
	{
		return priority < other.priority;
	}

	TaskStream::TaskStream()
		: streamIndex(0)
		, loopCount(0)
		, allocator("None")
	{
		Assert(threadID == std::thread::id());
	}

	TaskStream::TaskStream(StaticString name, TStreamIndex streamIndex)
		: name(name)
		, streamIndex(streamIndex)
		, loopCount(0)
		, allocator(name)
	{
		auto log = Logger::Get(name);
		log.Out([name = name](auto& ls) { ls << name.c_str() << " is created."; });
	}

	void TaskStream::Enqueue(const RangedTask& task)
	{
		std::scoped_lock<std::mutex> lock(queueLock);
		taskQueue.Push(task);
		cv.notify_one();
	}

	void TaskStream::Dequeue(std::optional<RangedTask>& outTask)
	{
		std::scoped_lock<std::mutex> lock(queueLock);
		if (taskQueue.IsEmpty())
		{
			outTask.reset();
			return;
		}

		outTask = taskQueue.Pop();
	}

	void TaskStream::WakeUp() { cv.notify_one(); }

	void TaskStream::Start(TaskSystem& taskSys)
	{
		auto func = [this]()
		{
			RunLoop();
		};

		thread = std::thread(func);
		OS::SetThreadPriority(thread, 0);
	}

	void TaskStream::RunLoop()
	{
		AllocatorScope scope(allocator);

		TaskSystem::SetThreadName(name);
		TaskSystem::SetStreamIndex(streamIndex);

		const auto log = Logger::Get(name);
		log.Out([name = name](auto& ls) { ls << name.c_str() << " has begun."; });

		threadID = std::this_thread::get_id();

		static ConfigParam<float, true> thresholdDuration("TaskStreamDurationThreshold",
			"Print a warning log if it detects slower task. (seconds)", 0.16f);

		auto& engine = Engine::Get();
		auto& taskSys = engine.GetTaskSystem();

		HVector<RangedTask> readdingBuffer;

		for (;likely(taskSys.IsRunning()); ++loopCount)
		{
			std::optional<RangedTask> rangedTask;

			{
				// Remove finished tasks
				std::unique_lock lock(queueLock);
				taskQueue.Remove([](const RangedTask& task) { return task.HasFinished(); });

				taskQueue.PushRange(readdingBuffer);
				readdingBuffer.clear();

				if (!taskQueue.IsEmpty())
				{
					rangedTask = taskQueue.Pop();
				}
			}

			if (!rangedTask.has_value())
			{
				taskSys.Dequeue(rangedTask);
			}

			if (!rangedTask.has_value())
			{
				// Wait for a signal for waitPeriod
				std::unique_lock lock(queueLock);
				constexpr std::chrono::milliseconds waitPeriod(10);
				cv.wait_for(lock, waitPeriod);

				continue;
			}

			time::TDuration duration;
			{
				time::ScopedTime timer(duration);
				rangedTask->Run();
			}

			const float deltaTime = time::ToFloat(duration);
			if (deltaTime > thresholdDuration.Get())
			{
				log.OutWarning([dt = deltaTime](auto& ls) { ls << "Slow DeltaTime = " << dt; });
			}

			if (!rangedTask->HasFinished())
			{
				readdingBuffer.push_back(*rangedTask);
			}
		}

		log.Out([name = name](auto& ls) { ls << name.c_str() << " has been terminated."; });
	}
} // namespace hbe
