// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <thread>
#include "Container/Array.h"
#include "Container/BoundedPriorityQueue.h"
#include "TaskRegistry.h"
#include "TaskStream.h"
#include "Time.h"

namespace hbe
{

/// API reference: docs/Core/TaskSystem/index.html
class TaskSystem final
{
public:
	using TThread = std::thread;
	using TThreadID = std::thread::id;

	using TStreamArray = Array<TaskStream>;
	using TIndex = TStreamArray::TIndex;

	using TMainThreadTask = void (*)(void*);

	static constexpr TIndex NonStreamIndex = static_cast<TIndex>(-1);
	static constexpr TIndex BaseStreamIndex = 0;
	static constexpr TIndex IOStreamIndex = 1;

	static constexpr TIndex MaxStreamsPerSplit = 64;

	static constexpr const char* EngineLoopThreadName = "EngineLoop";

private:
	std::atomic<bool> isRunning;

	const StaticString name;
	const TIndex numHardwareThreads;

	const TThreadID engineLoopThreadID;
	TThreadID ioTaskThreadID;

	TStreamArray streams;
	TaskRegistry taskRegistry;

	std::mutex taskQueueMutex;
	BoundedPriorityQueue<WorkItem> taskQueue;

	time::TTime lastBudgetWindowAdvance{};
	std::atomic<std::size_t> numBudgetWindowsAdvanced{0};

public:
	static TIndex GetNumHardwareThreads() noexcept;
	static void SetThreadName(StaticString name) noexcept;
	static void SetStreamIndex(TIndex index) noexcept;
	static TIndex GetCurrentStreamIndex() noexcept;
	static StaticString GetCurrentStreamName() noexcept;
	static StaticString GetCurrentThreadName() noexcept;
	static bool IsBaseThread() noexcept;
	static bool IsIOThread() noexcept;
	static TIndex GetBaseTaskStreamIndex() noexcept;
	static TIndex GetIOTaskStreamIndex() noexcept;

public:
	TaskSystem() noexcept;
	~TaskSystem() noexcept;

	void Initialize() noexcept;
	void RequestShutDown() noexcept;
	void RequestOtherStreamsClose() noexcept;
	void JoinAndClear() noexcept;
	void Update() noexcept;

	[[nodiscard]] bool AreOtherStreamsClosed() noexcept;
	[[nodiscard]] TaskID CreateTask(StaticString taskName, TRunnable func, void* userData) noexcept;
	[[nodiscard]] Task* FindTask(TaskID id) noexcept;
	[[nodiscard]] TaskID ParallelFor(StaticString taskName, TRunnable func, void* userData, TIndex numItems,
									 TIndex numSubJobs, const TIndex* streamIndices, TIndex numStreamIndices,
									 uint8_t priority = 0, TaskID successor = {},
									 TIndex successorStream = NonStreamIndex) noexcept;
	[[nodiscard]] TaskID ParallelFor(StaticString taskName, TRunnable func, void* userData, TIndex numItems,
									 TIndex numSubJobs, TIndex numStreams, uint8_t priority = 0, TaskID successor = {},
									 TIndex successorStream = NonStreamIndex) noexcept;

	[[nodiscard]] StaticString GetName() const noexcept;
	[[nodiscard]] bool IsRunning() const noexcept;
	[[nodiscard]] TaskRegistry& GetRegistry() noexcept;

	template <class Predicate>
	bool DriveUntil(const char* waitingFor, Predicate&& isDone,
					std::chrono::milliseconds patience = std::chrono::milliseconds(30000)) noexcept
	{
		TaskStream& baseStream = GetStream(GetBaseTaskStreamIndex());
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
			Update();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			remaining -= std::chrono::milliseconds(1);
		}

		if (!isDone())
		{
			ReportDriveTimeout(waitingFor, patience);
		}

		return isDone();
	}

	void ReportDriveTimeout(const char* waitingFor, std::chrono::milliseconds patience) noexcept;

	void RunBudgetWindowPass() noexcept;

	[[nodiscard]] std::size_t GetNumBudgetWindowsAdvanced() const noexcept;

	void Enqueue(const WorkItem& task) noexcept;
	void Dequeue(std::optional<WorkItem>& outTask) noexcept;

	void Enqueue(TIndex streamIndex, const WorkItem& task, StreamDrainPolicy::ELane lane) noexcept;
	void Enqueue(TIndex streamIndex, const WorkItem& task) noexcept;

	void EnqueueTask(TIndex streamIndex, Task& task, uint8_t priority = 0,
					 StreamDrainPolicy::ELane lane = StreamDrainPolicy::ELane::Fifo) noexcept;

	void DispatchToMainThread(TMainThreadTask task, void* userData, uint8_t priority = 128) noexcept;


	void ReleaseTask(TaskID id) noexcept;


	TaskID RunSplit(StaticString taskName, TRunnable func, void* userData, TIndex numItems, TIndex numSubJobs,
					const TIndex* streamIndices, TIndex numStreamIndices, uint8_t priority, TaskID successor,
					TIndex successorStream) noexcept;

	void SetSuccessor(TaskID task, TaskID successor) noexcept;
	[[nodiscard]] TaskID GetSuccessor(TaskID task) noexcept;

	void SetAbandonedNotice(TaskID task, FAbandonedNotice handler, void* userData = nullptr) noexcept;

	void DispatchSuccessor(TaskID finishedTask) noexcept;

	[[nodiscard]] StaticString GetStreamName(int index) const noexcept;
	[[nodiscard]] TIndex GetStreamIndex(TThreadID id) const noexcept;
	TaskStream& GetStream(int index) noexcept;

	[[nodiscard]] bool HasStream(TIndex index) const noexcept;

	TaskStream& GetBaseTaskStream() noexcept;
	const TaskStream& GetBaseTaskStream() const noexcept;

	TaskStream& GetIOTaskStream() noexcept;
	const TaskStream& GetIOTaskStream() const noexcept;

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
	TaskSystemTest()
		: TestCollection("TaskSystemTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
