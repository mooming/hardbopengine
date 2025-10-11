//
// Created by mooming on 11/10/2025.
//

#pragma once
#include <cstddef>
#include <functional>

#include "String/StaticString.h"

namespace hbe
{
	class Task;

	// A task can be split into multiple RangedTasks with ranges.
	// Task with a range [start, end).
	class RangedTask final
	{
		using TIndex = std::size_t;

	public:
		StaticString taskName;

		uint8_t priority;

		// Shared Task
		std::reference_wrapper<Task> taskRef;

		// RangedTask Start Index
		TIndex start;

		// RangedTask End Index
		TIndex end;

		// Index to be processed
		mutable TIndex currentIndex;

	public:
		~RangedTask() = default;
		RangedTask& operator=(const RangedTask& other) = default;

		bool operator<(const RangedTask& other) const { return priority < other.priority; }
		bool HasFinished() const { return currentIndex >= end; }

		// Run the runnable of the task. It'll call ReportFinishedSubTask when it's finished or been cancelled.
		void Run();

	private:
		RangedTask(Task& task, TIndex start, TIndex end, uint8_t priority);

		friend class Task;
		friend class TaskStream;
	};
} // namespace hbe
