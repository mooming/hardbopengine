// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "MainThreadTaskQueue.h"


namespace hbe
{

MainThreadTaskQueue::MainThreadTaskQueue()
: isRunning(true)
{
}

void MainThreadTaskQueue::enqueue(TTaskFunc taskFunc, void* userData, uint8_t priority) noexcept
{
TaskItem item(priority, taskFunc, userData);
queue.push(item);
}

size_t MainThreadTaskQueue::processTasks() noexcept
{
size_t processed = 0;

while (!queue.IsEmpty())
{
	auto itemOpt = queue.pop();
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
return !queue.IsEmpty();
}

void MainThreadTaskQueue::requestStop() noexcept
{
isRunning = false;
}

bool MainThreadTaskQueue::IsRunning() const noexcept
{
return isRunning;
}

} // namespace hbe
