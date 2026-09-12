// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


#include <atomic>
#include <thread>
#include "Container/Array.h"
#include "Container/BoundedPriorityQueue.h"
#include "MainThreadTaskQueue.h"
#include "TaskStream.h"

namespace hbe
{

/// @brief Manages tasks and task streams for parallel task execution.
class TaskSystem final
{
public:
	using TThread = std::thread;
	using TThreadID = std::thread::id;
	using TStreamArray = Array<TaskStream>;
	using TIndex = TStreamArray::TIndex;
	using TMainThreadTask = void (*)(void* /*userData*/);

	static constexpr TIndex BaseStreamIndex = 0;
	static constexpr TIndex IOStreamIndex = 1;

private:
	std::atomic<bool> isRunning;

	const StaticString name;
	const TIndex numHardwareThreads;
	const TThreadID baseTaskThreadID;
	TThreadID ioTaskThreadID;
	TStreamArray streams;

	std::mutex taskQueueMutex;
	BoundedPriorityQueue<RangedTask> taskQueue;

	MainThreadTaskQueue mainThreadTaskQueue;

public:
	static TIndex GetNumHardwareThreads() noexcept;
	static void SetThreadName(StaticString name) noexcept;
	static void SetStreamIndex(TIndex index) noexcept;
	static StaticString GetCurrentStreamName() noexcept;
	static StaticString GetCurrentThreadName() noexcept;
	static TIndex GetCurrentStreamIndex() noexcept;

	static bool IsBaseThread() noexcept;
	static bool IsIOThread() noexcept;
	static TIndex GetBaseTaskStreamIndex() noexcept { return BaseStreamIndex; }
	static TIndex GetIOTaskStreamIndex() noexcept { return IOStreamIndex; }

public:
	TaskSystem() noexcept;
	~TaskSystem() noexcept;

	void Initialize() noexcept;
	void RequestShutDown() noexcept;
	void JoinAndClear() noexcept;

	// Enqueue a task into the general task queue which is a low-priority queue. The task will be executed after
	// performing all existing special queue for eash task stream.
	void Enqueue(const RangedTask& task) noexcept;

	// Take the top priority task from the general task queue if task stream affinity has been set.
	// It'll add an task-stream affinity once it fails to take the top priority task due to its task-stream affinity
	// to prevent blocking the entire task streams by a task with null-affinity
	void Dequeue(std::optional<RangedTask>& outTask) noexcept;

	void Enqueue(TIndex streamIndex, const RangedTask& task) noexcept;

	// Dispatch a task to be executed on the main thread.
	// The task will be queued and executed when the main thread processes its queue.
	void DispatchToMainThread(TMainThreadTask task, void* userData, uint8_t priority = 0) noexcept;

	// Process all pending main thread tasks.
	size_t ProcessMainThreadTasks() noexcept;

	[[nodiscard]] StaticString GetName() const noexcept { return name; }
	[[nodiscard]] auto& IsRunning() const noexcept { return isRunning; }

	auto& GetBaseTaskStream() noexcept { return streams[GetBaseTaskStreamIndex()]; }
	auto& GetBaseTaskStream() const noexcept { return streams[GetBaseTaskStreamIndex()]; }
	auto& GetIOTaskStream() noexcept { return streams[GetIOTaskStreamIndex()]; }
	auto& GetIOTaskStream() const noexcept { return streams[GetIOTaskStreamIndex()]; }

	auto& GetMainThreadTaskQueue() noexcept { return mainThreadTaskQueue; }

	[[nodiscard]] StaticString GetStreamName(int index) const noexcept;
	[[nodiscard]] TIndex GetStreamIndex(TThreadID id) const noexcept;
	TaskStream& GetStream(int index) noexcept;

private:
	void BuildStreams();
};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

class TaskSystemTest : public TestCollection
{
public:
	TaskSystemTest() : TestCollection("TaskSystemTest") {}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
