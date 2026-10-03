// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once
#include <chrono>
#include <cstddef>
#include "TaskID.h"
#include "TaskStreamAffinity.h"

namespace hbe
{
class Task;

using FAbandonedNotice = void (*)(TaskID abandonedTask, void* userData) noexcept;

/// API reference: docs/Core/WorkItem/index.html
class WorkItem final
{
	using TIndex = std::size_t;

public:
	uint8_t priority;
	mutable TaskStreamAffinity affinity;

	TaskID taskID;

	TIndex start;
	TIndex end;
	mutable TIndex current;

	FAbandonedNotice abandonedNotice{nullptr};

	void* abandonedUserData{nullptr};

	std::chrono::nanoseconds offerTime{};

public:
	friend class TaskSystemTest;

	~WorkItem() = default;
	WorkItem& operator=(const WorkItem& other) = default;

	bool operator<(const WorkItem& other) const noexcept
	{
		return priority < other.priority;
	}

	[[nodiscard]] bool HasFinished() const noexcept
	{
		return current >= end;
	}

	bool Run(Task& task) noexcept;

private:
	WorkItem(Task& task, TIndex start, TIndex end, uint8_t priority) noexcept;

	friend class Task;
	friend class TaskStream;
};
} // namespace hbe
