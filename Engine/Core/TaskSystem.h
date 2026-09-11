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
	static TIndex getNumHardwareThreads() noexcept;
	static void setThreadName(StaticString name) noexcept;
	static void setStreamIndex(TIndex index) noexcept;
	static StaticString getCurrentStreamName() noexcept;
	static StaticString getCurrentThreadName() noexcept;
	static TIndex getCurrentStreamIndex() noexcept;

	static bool isBaseThread() noexcept;
	static bool isIOThread() noexcept;
	static TIndex getBaseTaskStreamIndex() noexcept { return BaseStreamIndex; }
	static TIndex getIOTaskStreamIndex() noexcept { return IOStreamIndex; }

public:
	TaskSystem() noexcept;
	~TaskSystem() noexcept;

	void initialize() noexcept;
	void requestShutDown() noexcept;
	void joinAndClear() noexcept;

	// Enqueue a task into the general task queue which is a low-priority queue. The task will be executed after
	// performing all existing special queue for eash task stream.
	void enqueue(const RangedTask& task) noexcept;

	// Take the top priority task from the general task queue if task stream affinity has been set.
	// It'll add an task-stream affinity once it fails to take the top priority task due to its task-stream affinity
	// to prevent blocking the entire task streams by a task with null-affinity
	void dequeue(std::optional<RangedTask>& outTask) noexcept;

	void enqueue(TIndex streamIndex, const RangedTask& task) noexcept;

	// Dispatch a task to be executed on the main thread.
	// The task will be queued and executed when the main thread processes its queue.
	void dispatchToMainThread(TMainThreadTask task, void* userData, uint8_t priority = 0) noexcept;

	// Process all pending main thread tasks.
	size_t processMainThreadTasks() noexcept;

	[[nodiscard]] StaticString getName() const noexcept { return name; }
	[[nodiscard]] auto& IsRunning() const noexcept { return isRunning; }

	auto& getBaseTaskStream() noexcept { return streams[getBaseTaskStreamIndex()]; }
	auto& getBaseTaskStream() const noexcept { return streams[getBaseTaskStreamIndex()]; }
	auto& getIOTaskStream() noexcept { return streams[getIOTaskStreamIndex()]; }
	auto& getIOTaskStream() const noexcept { return streams[getIOTaskStreamIndex()]; }

	auto& GetMainThreadTaskQueue() noexcept { return mainThreadTaskQueue; }

	[[nodiscard]] StaticString getStreamName(int index) const noexcept;
	[[nodiscard]] TIndex getStreamIndex(TThreadID id) const noexcept;
	TaskStream& getStream(int index) noexcept;

private:
	void buildStreams();
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
	void prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
