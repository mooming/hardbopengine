// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "MainThreadTaskQueue.h"

#include <mutex>
#include <optional>

namespace hbe
{

MainThreadTaskQueue::MainThreadTaskQueue()
	: isRunning(true)
{
}

void MainThreadTaskQueue::Enqueue(TTaskFunc taskFunc, void* userData, uint8_t priority) noexcept
{
	TaskItem item(priority, taskFunc, userData);
	std::lock_guard lock(queueLock);
	queue.Push(item);
}

size_t MainThreadTaskQueue::ProcessTasks() noexcept
{
	size_t processed = 0;

	for (;;)
	{
		std::optional<TaskItem> itemOpt;
		{
			std::lock_guard lock(queueLock);
			itemOpt = queue.Pop();
		}

		if (!itemOpt.has_value())
		{
			break;
		}

		TaskItem& item = *itemOpt;
		if (item.taskFunc)
		{
			item.taskFunc(item.userData);
			++processed;
		}
	}

	return processed;
}

bool MainThreadTaskQueue::HasPendingTasks() const noexcept
{
	std::lock_guard lock(queueLock);
	return !queue.IsEmpty();
}

void MainThreadTaskQueue::RequestStop() noexcept
{
	isRunning = false;
}

bool MainThreadTaskQueue::IsRunning() const noexcept
{
	return isRunning;
}

} // namespace hbe
