// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Task.h"

#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "TaskSystem.h"

namespace hbe
{

Task::Task() noexcept
	: numSubTasks(0)
	, numGeneratedSubTasks(0)
	, numFinishedSubTasks(0)
	, func(nullptr)
	, userData(nullptr)
{
}

Task::Task(StaticString taskName, TRunnable func, void* userData) noexcept
	: name(taskName)
	, numSubTasks(0)
	, numGeneratedSubTasks(0)
	, numFinishedSubTasks(0)
	, func(func)
	, userData(userData)
{
}

void Task::BusyWait() const noexcept
{
	while (!HasDone())
		;
}

void Task::Wait(uint32_t intervalMilliSecs) const noexcept
{
	const auto interval = std::chrono::milliseconds(intervalMilliSecs);

	while (!HasDone())
	{
		std::this_thread::sleep_for(interval);
	}
}

void Task::LoadIntoRecord(TaskID newID, StaticString taskName, TRunnable newFunc, void* newUserData) noexcept
{
	id = newID;
	name = taskName;
	numSubTasks = 0;
	numGeneratedSubTasks = 0;
	result.Clear();
	numFinishedSubTasks.store(0, std::memory_order::relaxed);
	func = newFunc;
	userData = newUserData;
}

RangedTask Task::GenerateSubTask(TIndex start, TIndex end, uint8_t priority) noexcept
{
	Assert(numGeneratedSubTasks < numSubTasks,
		   "\"%s\" handed out a work item beyond the %d it reserved, so this task reports itself finished before"
		   " the item runs.",
		   name.c_str(), numSubTasks);

	return {*this, start, end, priority};
}
} // namespace hbe
