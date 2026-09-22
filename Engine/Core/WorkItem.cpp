// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "WorkItem.h"

#include "Log/Logger.h"
#include "TaskSystem.h"

namespace hbe
{
bool WorkItem::Run(Task& task) noexcept
{
	auto runnable = task.GetRunnable();
	if (runnable == nullptr)
	{
		current = end;

		auto logger = Logger::Get(task.GetName());
		logger.OutError([](auto& ls) { ls << "Null Runnable."; });

		return false;
	}

	auto userData = task.GetUserData();
	auto delta = runnable(userData, current, end);
	current += delta;

	if (!HasFinished())
	{
		return false;
	}

	return task.ReportFinishedSubTask();
}

WorkItem::WorkItem(Task& task, TIndex start, TIndex end, uint8_t priority) noexcept
	: priority(priority)
	, taskID(task.GetID())
	, start(start)
	, end(end)
	, current(start)
{
	affinity.Unset(TaskSystem::GetBaseTaskStreamIndex());
	affinity.Unset(TaskSystem::GetIOTaskStreamIndex());
}
} // namespace hbe
