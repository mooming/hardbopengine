// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>
#include <mutex>

#include "Container/BoundedPriorityQueue.h"


namespace hbe
{
/// API reference: docs/Core/MainThreadTaskQueue/index.html
class MainThreadTaskQueue final
{
public:
	using TTaskFunc = void (*)(void*);

private:
	struct TaskItem
	{
		uint8_t priority;
		TTaskFunc taskFunc;
		void* userData;

		TaskItem(uint8_t p, TTaskFunc t, void* userData)
			: priority(p)
			, taskFunc(t)
			, userData(userData)
		{
		}
	};

	static constexpr size_t MaxQueueSize = 1024;
	using TQueue = BoundedPriorityQueue<TaskItem, 256, MaxQueueSize>; // hb-standards:ignore

	TQueue queue;
	mutable std::mutex queueLock;

public:
	MainThreadTaskQueue();
	~MainThreadTaskQueue() = default;

	void Enqueue(TTaskFunc taskFunc, void* userData, uint8_t priority = 128) noexcept;
	size_t ProcessTasks() noexcept;
	[[nodiscard]] bool HasPendingTasks() const noexcept;
};
} // namespace hbe
