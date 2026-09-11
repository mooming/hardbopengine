// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Task.h"

#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "TaskSystem.h"


namespace hbe
{

Task::Task() noexcept
	: numSubTasks(0)
	, numFinishedSubTasks(0)
	, func(nullptr)
	, userData(nullptr)
{
}

Task::Task(StaticString taskName, TRunnable func, void* userData) noexcept
	: name(taskName)
	, numSubTasks(0)
	, numFinishedSubTasks(0)
	, func(func)
	, userData(userData)
{}

void Task::start(TIndex numberOfSubTasks, TIndex startIndex, TIndex endIndex, uint8_t priority) noexcept
{
	auto& taskSystem = Engine::get().getTaskSystem();

	if (endIndex <= startIndex)
	{
		auto rangedTask = generateSubTask(startIndex, endIndex, priority);
		taskSystem.enqueue(rangedTask);
		return;
	}

	endIndex = std::max(startIndex, endIndex);
	if (numberOfSubTasks < 2)
	{
		// Single Thread Task
		const auto rangedTask = generateSubTask(startIndex, endIndex, priority);
		taskSystem.enqueue(rangedTask);

		return;
	}

	constexpr TIndex one = 1;
	const TIndex length = endIndex - startIndex + 1;
	TIndex interval = length / numSubTasks; // always bigger than zero.
	interval = std::max(interval, one);
	TIndex iStart = startIndex;
	TIndex iEnd = iStart + interval;

	while (iEnd < endIndex)
	{
		auto rangedTask = generateSubTask(iStart, iEnd, priority);
		taskSystem.enqueue(rangedTask);

		iStart = iEnd;
		iEnd += interval;
	}

	auto rangedTask = generateSubTask(iStart, endIndex, priority);
	taskSystem.enqueue(rangedTask);
}

void Task::busyWait() const noexcept
{
	while (!HasDone());
}

void Task::wait(uint32_t intervalMilliSecs) const noexcept
{
	const auto interval = std::chrono::milliseconds(intervalMilliSecs);

	while (!HasDone())
	{
		std::this_thread::sleep_for(interval);
	}
}

RangedTask Task::generateSubTask(TIndex start, TIndex end, uint8_t priority) noexcept
{
	++numSubTasks;
	return {*this, start, end, priority};
}
} // namespace hbe
