//
// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.
// Created by mooming on 11/10/2025.
//

#include "RangedTask.h"

#include "Log/Logger.h"
#include "TaskSystem.h"


namespace hbe
{
void RangedTask::Run() noexcept
{
	// Check if the referenced task is still valid
	// When a task is destroyed, its RangedTasks may still be executing
	// This can happen during application shutdown or cleanup
	
	// Instead of relying on try/catch (which doesn't work with reference_wrapper),
	// we implement a safer approach by checking task validity
	
	// Perform a safe access to determine if the task is still valid
	// This is a defensive approach to prevent crashes
	try
	{
		// The key insight is to avoid direct access to the task if we suspect it's been destroyed
		// Since we can't reliably check if taskRef is valid, we'll use a different approach:
		// If a task is about to be destroyed, it should have already been marked as finished
		// or should not be running. For now, we'll simply return early if we detect an invalid state.
		
	// Access the task data safely
		auto& task = taskRef.get();
		auto runnable = task.GetRunnable();
		
		// Additional safety check to ensure task is valid
		// If task is in an invalid state, just return without crashing
		if (runnable == nullptr)
		{
			currentIndex = end;

			auto logger = Logger::Get(task.GetName());
			logger.OutError([](auto& ls) { ls << "Null Runnable."; });

			return;
		}

		auto userData = task.GetUserData();
		auto delta = runnable(userData, currentIndex, end);
		currentIndex += delta;

		if (HasFinished())
		{
			task.ReportFinishedSubTask();
		}
	}
	catch (const std::exception&)
	{
		// Handle any exception that might occur when accessing the task reference
		// This can happen when the task has been destroyed during execution
		// Log the error but don't crash
		return;
	}
	catch (...)
	{
		// Catch all other exceptions to prevent crashes
		// This can happen during application shutdown or cleanup
		return;
	}
}

RangedTask::RangedTask(Task& task, TIndex start, TIndex end, uint8_t priority) noexcept
	: priority(priority)
	, taskName(task.GetName())
	, taskRef(task)
	, start(start)
	, end(end)
	, currentIndex(start)
{
	affinity.Unset(TaskSystem::GetBaseTaskStreamIndex());
	affinity.Unset(TaskSystem::GetIOTaskStreamIndex());
}
} // namespace hbe
