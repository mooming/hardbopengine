// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Task.h"

#include "Time.h"
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
	abandonedNotice = nullptr;
	abandonedUserData = nullptr;
	offerTime = std::chrono::duration_cast<std::chrono::nanoseconds>(time::ElapsedSinceEngineEpoch());
}

WorkItem Task::GenerateSubTask(TIndex start, TIndex end, uint8_t priority) noexcept
{
	Assert(numGeneratedSubTasks < numSubTasks,
		   "\"%s\" handed out a work item beyond the %d it reserved, so this task reports itself finished before"
		   " the item runs.",
		   name.c_str(), numSubTasks);

	return {*this, start, end, priority};
}
} // namespace hbe
