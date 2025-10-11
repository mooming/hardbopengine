// Created by mooming.go@gmail.com, 2022

#include "Task.h"

#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "TaskSystem.h"


namespace hbe
{

	Task::Task()
		: numSubTasks(0)
		, numFinishedSubTasks(0)
		, func(nullptr)
		, userData(nullptr)
	{
	}

	Task::Task(StaticString taskName, Runnable func, void* userData)
		: name(taskName)
		, numSubTasks(0)
		, numFinishedSubTasks(0)
		, func(func)
		, userData(userData)
	{}

	void Task::Start(TIndex numberOfSubTasks, TIndex startIndex, TIndex endIndex, uint8_t priority)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();

		if (endIndex <= startIndex)
		{
			auto rangedTask = GenerateSubTask(startIndex, endIndex, priority);
			taskSystem.Enqueue(rangedTask);
			return;
		}

		endIndex = std::max(startIndex, endIndex);
		if (numberOfSubTasks < 2)
		{
			// Single Thread Task
			const auto rangedTask = GenerateSubTask(startIndex, endIndex, priority);
			taskSystem.Enqueue(rangedTask);

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
			auto rangedTask = GenerateSubTask(iStart, iEnd, priority);
			taskSystem.Enqueue(rangedTask);

			iStart = iEnd;
			iEnd += interval;
		}

		auto rangedTask = GenerateSubTask(iStart, endIndex, priority);
		taskSystem.Enqueue(rangedTask);
	}

	void Task::BusyWait() const
	{
		while (!HasDone());
	}

	void Task::Wait(uint32_t intervalMilliSecs) const
	{
		const auto interval = std::chrono::milliseconds(intervalMilliSecs);

		while (!HasDone())
		{
			std::this_thread::sleep_for(interval);
		}
	}

	RangedTask Task::GenerateSubTask(TIndex start, TIndex end, uint8_t priority)
	{
		++numSubTasks;
		return {*this, start, end, priority};
	}
} // namespace hbe
