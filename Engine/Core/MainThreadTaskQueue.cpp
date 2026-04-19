// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "MainThreadTaskQueue.h"

namespace hbe
{

MainThreadTaskQueue::MainThreadTaskQueue()
	: isRunning(true)
{
}

void MainThreadTaskQueue::Enqueue(TTaskFunc taskFunc, void* userData, uint8_t priority)
{
	TaskItem item(priority, taskFunc, userData);
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
		if (item.taskFunc)
		{
			item.taskFunc(item.userData);
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
