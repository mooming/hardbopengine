// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

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
	auto log = Logger::get(name);
	log.out([name = name](auto& ls) { ls << name.c_str() << " is created."; });
}

void TaskStream::enqueue(const RangedTask& task) noexcept
{
	std::scoped_lock<std::mutex> lock(queueLock);
	taskQueue.push(task);
	cv.notify_one();
}

void TaskStream::dequeue(std::optional<RangedTask>& outTask)
{
	std::scoped_lock<std::mutex> lock(queueLock);
	if (taskQueue.IsEmpty())
	{
		outTask.reset();
		return;
	}

	outTask = taskQueue.pop();
}

void TaskStream::wakeUp() noexcept { cv.notify_one(); }

void TaskStream::start(TaskSystem& taskSys) noexcept
{
	auto func = [this]()
	{
		runLoop();
	};

	thread = std::thread(func);
	OS::SetThreadPriority(thread, 0);
}

void TaskStream::runLoop() noexcept
{
	AllocatorScope scope(allocator);

	TaskSystem::setThreadName(name);
	TaskSystem::setStreamIndex(streamIndex);

	const auto log = Logger::get(name);
	log.out([name = name](auto& ls) { ls << name.c_str() << " has begun."; });

	threadID = std::this_thread::get_id();

	static ConfigParam<float, true> thresholdDuration("TaskStreamDurationThreshold",
		"Print a warning log if it detects slower task. (seconds)", 0.16f);

	auto& engine = Engine::get();
	auto& taskSys = engine.getTaskSystem();

	HVector<RangedTask> readdingBuffer;

	for (;likely(taskSys.IsRunning()); ++loopCount)
	{
		std::optional<RangedTask> rangedTask;

		{
			// Remove finished tasks
			std::unique_lock lock(queueLock);
			taskQueue.Remove([](const RangedTask& task) { return task.hasFinished(); });

			taskQueue.pushRange(readdingBuffer);
			readdingBuffer.clear();

			if (!taskQueue.IsEmpty())
			{
				rangedTask = taskQueue.pop();
			}
		}

		if (!rangedTask.has_value())
		{
			taskSys.dequeue(rangedTask);
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
			rangedTask->run();
		}

		const float deltaTime = time::toFloat(duration);
		if (deltaTime > thresholdDuration.get())
		{
			log.outWarning([dt = deltaTime](auto& ls) { ls << "Slow DeltaTime = " << dt; });
		}

		if (!rangedTask->hasFinished())
		{
			readdingBuffer.push_back(*rangedTask);
		}
	}

	log.out([name = name](auto& ls) { ls << name.c_str() << " has been terminated."; });
}
} // namespace hbe
