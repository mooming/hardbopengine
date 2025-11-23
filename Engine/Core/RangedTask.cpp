//
// Created by mooming on 11/10/2025.
//

#include "RangedTask.h"

#include "Log/Logger.h"
#include "TaskSystem.h"

namespace hbe
{
	void RangedTask::Run()
	{
		auto& task = taskRef.get();
		auto runnable = task.GetRunnable();
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

	RangedTask::RangedTask(Task& task, TIndex start, TIndex end, uint8_t priority)
		: priority(priority)
		, taskName(task.GetName())
		, taskRef(task)
		, start(start)
		, end(end)
		, currentIndex(start)
	{
		affinity.Unset(TaskSystem::GetMainTaskStreamIndex());
		affinity.Unset(TaskSystem::GetIOTaskStreamIndex());
	}
} // namespace hbe
