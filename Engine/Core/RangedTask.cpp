//
// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.
// Created by mooming on 11/10/2025.
//

#include "RangedTask.h"

#include "Log/Logger.h"
#include "TaskSystem.h"


namespace hbe
{
void RangedTask::run() noexcept
{
	auto& task = taskRef.get();
	auto runnable = task.getRunnable();
	if (runnable == nullptr)
	{
		currentIndex = end;

		auto logger = Logger::get(task.getName());
		logger.outError([](auto& ls) { ls << "Null Runnable."; });

		return;
	}

	auto userData = task.getUserData();
	auto delta = runnable(userData, currentIndex, end);
	currentIndex += delta;

	if (hasFinished())
	{
		task.reportFinishedSubTask();
	}
}

RangedTask::RangedTask(Task& task, TIndex start, TIndex end, uint8_t priority) noexcept
	: priority(priority)
	, taskName(task.getName())
	, taskRef(task)
	, start(start)
	, end(end)
	, currentIndex(start)
{
	affinity.unset(TaskSystem::getBaseTaskStreamIndex());
	affinity.unset(TaskSystem::getIOTaskStreamIndex());
}
} // namespace hbe
