// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <thread>

#include "Core/TaskStream.h"
#include "Core/TaskSystem.h"

namespace hbe
{
namespace TestHelper
{
void ReportDriveTimeout(TaskSystem& taskSystem, const char* waitingFor, std::chrono::milliseconds patience) noexcept;

template <class Predicate>
bool DriveUntil(TaskSystem& taskSystem, const char* waitingFor, Predicate&& isDone,
				std::chrono::milliseconds patience = std::chrono::milliseconds(30000)) noexcept
{
	TaskStream& baseStream = taskSystem.GetStream(TaskSystem::GetBaseTaskStreamIndex());
	auto remaining = patience;

	if (std::this_thread::get_id() != baseStream.GetThreadID())
	{
		while (!isDone() && remaining > std::chrono::milliseconds::zero())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			remaining -= std::chrono::milliseconds(1);
		}

		return isDone();
	}

	baseStream.SetNestedPumpAllowed(true);

	struct NestedPumpGuard
	{
		TaskStream& stream;

		~NestedPumpGuard()
		{
			stream.SetNestedPumpAllowed(false);
		}
	};

	NestedPumpGuard nestedPumpGuard{baseStream};

	while (!isDone() && remaining > std::chrono::milliseconds::zero())
	{
		taskSystem.Update();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		remaining -= std::chrono::milliseconds(1);
	}

	if (!isDone())
	{
		ReportDriveTimeout(taskSystem, waitingFor, patience);
	}

	return isDone();
}

} // namespace TestHelper
} // namespace hbe
