// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "ResultPacket.h"
#include "Runnable.h"
#include "String/StaticString.h"
#include "TaskID.h"
#include "WorkItem.h"


namespace hbe
{
class TaskRegistry;

/// API reference: docs/Core/Task/index.html
class Task final
{
public:
	using TIndex = std::size_t;
	using TThreadID = std::thread::id;
	using TNumSubTasks = uint8_t;

	static constexpr TNumSubTasks MaxNumSubTasks = 255;

private:
	TaskID id;
	StaticString name;

	TNumSubTasks numSubTasks;
	TNumSubTasks numGeneratedSubTasks;
	std::atomic<TNumSubTasks> numFinishedSubTasks;

	TRunnable func;
	void* userData;

	FAbandonedNotice abandonedNotice{nullptr};
	void* abandonedUserData{nullptr};

	std::chrono::nanoseconds offerTime{};

	ResultPacket result;

public:
	Task() noexcept;
	Task(StaticString taskName, TRunnable func, void* userData) noexcept;
	~Task() = default;

	void ReserveSubTasks(TNumSubTasks count) noexcept
	{
		numSubTasks = count;
	}

	[[nodiscard]] auto GetName() const noexcept
	{
		return name;
	}

	[[nodiscard]] TaskID GetID() const noexcept
	{
		return id;
	}

	[[nodiscard]] auto NumSubTasks() const noexcept
	{
		return numSubTasks;
	}

	[[nodiscard]] ResultPacket& GetResult() noexcept
	{
		return result;
	}

	[[nodiscard]] const ResultPacket& GetResult() const noexcept
	{
		return result;
	}

	[[nodiscard]] auto NumFinishedSubTasks() const noexcept
	{
		return numFinishedSubTasks.load(std::memory_order::relaxed);
	}

public:
	bool ReportFinishedSubTask() noexcept
	{
		const auto finishedBefore = numFinishedSubTasks.fetch_add(1, std::memory_order::seq_cst);

		return finishedBefore + 1 == numSubTasks;
	}

	[[nodiscard]] TRunnable GetRunnable() const noexcept
	{
		return func;
	}

	void SetRunnable(TRunnable runnable) noexcept
	{
		func = runnable;
	}

	[[nodiscard]] void* GetUserData() const noexcept
	{
		return userData;
	}

private:
	friend class TaskRegistry;
	friend class WorkItem;
	friend class TaskSystem;
	friend class TaskProvider;
	friend class TaskSystemTest;
	friend class TaskRegistryTest;

	[[nodiscard]] bool HasDone() const noexcept
	{
		return numSubTasks > 0 && NumFinishedSubTasks() >= numSubTasks;
	}

	void LoadIntoRecord(TaskID newID, StaticString taskName, TRunnable newFunc, void* newUserData) noexcept;

private:
	WorkItem GenerateSubTask(TIndex start, TIndex end, uint8_t priority = 0) noexcept;
};
} // namespace hbe
