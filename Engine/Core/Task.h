// Created by mooming.go@gmail.com, 2022

#pragma once

#include <atomic>
#include <thread>
#include "RangedTask.h"
#include "Runnable.h"
#include "String/StaticString.h"

namespace hbe
{
	// It represents a task which consists of RangedTasks. The task can be split into multiple RangedTasks on multiple threads.
	class Task final
	{
	public:
		using TIndex = std::size_t;
		using TThreadID = std::thread::id;
		using TNumSubTasks = uint8_t;

	private:
		// Task Name
		StaticString name;

		// Number of RangedTasks
		TNumSubTasks numSubTasks;

		// Number of finished RangedTasks
		std::atomic<TNumSubTasks> numFinishedSubTasks;

		// Runnable Function
		Runnable func;

		// Custom User Data
		void* userData;

	public:
		Task();
		Task(StaticString taskName, Runnable func, void* userData);
		~Task() = default;

		void Start(TIndex numberOfSubTasks, TIndex startIndex, TIndex endIndex, uint8_t priority = 0);

		// Wait
		void BusyWait() const;
		void Wait(uint32_t intervalMilliSecs = 10) const;

	public:
		auto GetName() const { return name; }
		auto NumSubTasks() const { return numSubTasks; }
		auto NumFinishedSubTasks() const { return numFinishedSubTasks.load(std::memory_order::relaxed); }
		bool HasDone() const { return numSubTasks > 0 && NumFinishedSubTasks() >= numSubTasks; }

		// Increase numFinishedSubTasks.  It guarantees all other global memory values are synced properly.
		void ReportFinishedSubTask() { numFinishedSubTasks.fetch_add(1, std::memory_order::seq_cst); }

		Runnable GetRunnable() const { return func;}
		void SetRunnable(Runnable runnable) { func = runnable; }
		void* GetUserData() const { return userData; }

		// Generate a RangedTask with the given range [start, end)
		RangedTask GenerateSubTask(TIndex start, TIndex end, uint8_t priority = 0);
	};
} // namespace hbe
