// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "MainThreadTaskQueue.h"

namespace hbe
{

MainThreadTaskQueue::MainThreadTaskQueue()
	: isRunning(true)
{
}

void MainThreadTaskQueue::Enqueue(Task task, uint8_t priority)
{
	TaskItem item(priority, std::move(task));
	queue.Push(item);
}

size_t MainThreadTaskQueue::ProcessTasks()
{
	size_t processed = 0;

	while (!queue.IsEmpty())
	{
		auto itemOpt = queue.Pop();
		if (!itemOpt.has_value())
		{
			break;
		}

		TaskItem& item = *itemOpt;
		if (item.task)
		{
			item.task();
			++processed;
		}
	}

	return processed;
}

bool MainThreadTaskQueue::HasPendingTasks() const
{
	return !queue.IsEmpty();
}

void MainThreadTaskQueue::RequestStop()
{
	isRunning = false;
}

bool MainThreadTaskQueue::IsRunning() const
{
	return isRunning;
}

} // namespace hbe
