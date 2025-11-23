// Created by mooming.go@gmail.com, 2022

#pragma once

#include <atomic>
#include <thread>
#include "Container/Array.h"
#include "HSTL/HVector.h"
#include "TaskStream.h"

namespace hbe
{

	// TaskSystem is a class to manage tasks and task streams.
	class TaskSystem final
	{
	public:
		using Thread = std::thread;
		using ThreadID = std::thread::id;
		using StreamArray = Array<TaskStream>;
		using TIndex = StreamArray::TIndex;

		static constexpr TIndex MainStreamIndex = 0;
		static constexpr TIndex IOStreamIndex = 1;

	private:
		std::atomic<bool> isRunning;

		const StaticString name;
		const TIndex numHardwareThreads;
		const ThreadID mainTaskThreadID;
		ThreadID ioTaskThreadID;
		StreamArray streams;

		std::mutex taskQueueMutex;
		std::priority_queue<RangedTask, HVector<RangedTask>> taskQueue;

	public:
		static TIndex GetNumHardwareThreads();
		static void SetThreadName(StaticString name);
		static void SetStreamIndex(TIndex index);
		static StaticString GetCurrentStreamName();
		static StaticString GetCurrentThreadName();
		static TIndex GetCurrentStreamIndex();

		static bool IsMainThread();
		static bool IsIOThread();
		static TIndex GetMainTaskStreamIndex() { return MainStreamIndex; }
		static TIndex GetIOTaskStreamIndex() { return IOStreamIndex; }

	public:
		TaskSystem();
		~TaskSystem();

		void Initialize();
		void RequestShutDown();
		void JoinAndClear();

		// Enqueue a task into the general task queue which is a low-priority queue. The task will be executed after
		// performing all existing special queue for eash task stream.
		void Enqueue(const RangedTask& task);

		// Take the top priority task from the general task queue if task stream affinity has been set.
		// It'll add an task-stream affinity once it fails to take the top priority task due to its task-stream affinity
		// to prevent blocking the entire task streams by a task with null-affinity
		void Dequeue(std::optional<RangedTask>& outTask);

		void Enqueue(TIndex streamIndex, const RangedTask& task);

		StaticString GetName() const { return name; }
		auto& IsRunning() const { return isRunning; }

		auto& GetMainTaskStream() { return streams[GetMainTaskStreamIndex()]; }
		auto& GetMainTaskStream() const { return streams[GetMainTaskStreamIndex()]; }
		auto& GetIOTaskStream() { return streams[GetIOTaskStreamIndex()]; }
		auto& GetIOTaskStream() const { return streams[GetIOTaskStreamIndex()]; }

		StaticString GetStreamName(int index) const;
		TIndex GetStreamIndex(ThreadID id) const;
		TaskStream& GetStream(int index);

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
