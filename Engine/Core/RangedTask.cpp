//
// Created by mooming on 11/10/2025.
//

#include "RangedTask.h"

#include "Log/Logger.h"


namespace hbe
{
	RangedTask::RangedTask(Task& task, TIndex start, TIndex end, uint8_t priority)
		: taskName(task.GetName()), priority(priority), taskRef(task), start(start), end(end), currentIndex(start)
	{
	}

	void RangedTask::Run()
	{
		auto& task = taskRef.get();
		auto runnable = task.GetRunnable();
		if (runnable == nullptr)
		{
			currentIndex = end;

			auto logger = Logger::Get(task.GetName());
			logger.OutError([](auto& ls){ ls << "Null Runnable.";});

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
} // namespace hbe
