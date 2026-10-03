// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "Container/Array.h"
#include "Container/BoundedPriorityQueue.h"
#include "Container/Deque.h"
#include "Core/CPUBudget.h"
#include "Core/MainThreadTaskQueue.h"
#include "Core/StreamDrainPolicy.h"
#include "HSTL/HVector.h"
#include "Memory/MultiPoolAllocator.h"
#include "String/StaticString.h"
#include "Task.h"
#include "TaskStreamIndex.h"
#include "WorkItem.h"


namespace hbe
{
class TaskProvider;

class TaskStream;
class TaskSystem;

/// API reference: docs/Core/TaskStream/index.html
class TaskStream final
{
private:
	template <typename T>
	using TVector = hbe::HVector<T>;
	using TIndex = Task::TIndex;
	using TStreamIndex = hbe::TStreamIndex;
	using TThreadID = std::thread::id;
	using TWorkItems = TVector<WorkItem>;

private:
	struct TaskQueueItem final
	{
		uint8_t priority;
		mutable WorkItem task;
		float duration;

		TaskQueueItem(uint8_t priority, const WorkItem& task);

		TaskQueueItem& operator=(const TaskQueueItem& other) = default;
		bool operator<(const TaskQueueItem& other) const;
	};

	static constexpr TIndex MaxProvidersPerLane = 8;

	struct LaneProviders final // hb-standards:ignore
	{
		std::array<TaskProvider*, static_cast<size_t>(MaxProvidersPerLane)> items{};
		TIndex count = 0;
	};

	std::array<LaneProviders, 2> laneProviders;

	StaticString name;
	TThreadID threadID;
	TStreamIndex streamIndex;

	std::uint64_t loopCount;

	MultiPoolAllocator allocator;

	TaskSystem* taskSystem = nullptr;

	HVector<WorkItem> readdingFifo;
	HVector<WorkItem> readdingPriority;
	CPUBudget budget;

	std::mutex queueLock;
	std::condition_variable cv;

	std::thread thread;

	Deque<WorkItem> fifoQueue;
	BoundedPriorityQueue<WorkItem> priorityQueue;

	StreamDrainPolicy drainPolicy;

	std::atomic<bool> windowAdvanceRequested{false};

	std::atomic<bool> closeRequested{false};
	std::atomic<bool> isClosed{false};

	std::uint64_t drivenPassCount = 0;

	MainThreadTaskQueue postedTasks;

	bool isPumping = false;
	bool nestedPumpAllowed = false;

	bool isDrainingForShutdown = false;

	std::atomic<std::size_t> abandonedWorkNoticeCount{0};
	std::atomic<std::size_t> agedOutWorkCount{0};

	bool drivenByShutdownPump{false};

	std::chrono::milliseconds shutdownDrainDeadline{2000};

	std::atomic<unsigned> generalQueueRefusals{0};
	std::atomic<unsigned> laneWorkRefusals{0};
	std::atomic<unsigned> providerAsksWhileSpent{0};

public:
	TaskStream();
	explicit TaskStream(StaticString name, TStreamIndex streamIndex);
	~TaskStream() = default;

	void EnqueueFifo(const WorkItem& task) noexcept;
	void EnqueuePriority(const WorkItem& task) noexcept;

	void ConfigureRate(uint32_t fifoWeight, uint32_t priorityWeight) noexcept;

	void SetMaxAge(std::chrono::nanoseconds maxAge) noexcept
	{
		drainPolicy.SetMaxAge(maxAge);
	}

	[[nodiscard]] std::chrono::nanoseconds GetMaxAge() const noexcept
	{
		return drainPolicy.GetMaxAge();
	}

	[[nodiscard]] uint32_t GetFifoWeight() const noexcept
	{
		return drainPolicy.GetFifoWeight();
	}

	[[nodiscard]] uint32_t GetPriorityWeight() const noexcept
	{
		return drainPolicy.GetPriorityWeight();
	}

	void WakeUp() noexcept;

	void ConfigureBudget(std::chrono::duration<double> allowance) noexcept;
	void RequestBudget(std::chrono::duration<double> allowance) noexcept;

	[[nodiscard]] bool MayTakeNewWork() const noexcept;
	[[nodiscard]] std::chrono::nanoseconds GetAccumulatedCPUTime() const noexcept;

	void RequestWindowAdvance() noexcept
	{
		windowAdvanceRequested.store(true, std::memory_order_relaxed);
	}

	[[nodiscard]] unsigned GetLaneWorkRefusalCount() const noexcept
	{
		return laneWorkRefusals.load(std::memory_order_relaxed);
	}

	[[nodiscard]] unsigned GetProviderAskWhileSpentCount() const noexcept
	{
		return providerAsksWhileSpent.load(std::memory_order_relaxed);
	}

	[[nodiscard]] unsigned GetGeneralQueueRefusalCount() const noexcept
	{
		return generalQueueRefusals.load(std::memory_order_relaxed);
	}

	void Join() noexcept
	{
		thread.join();
	}

	[[nodiscard]] auto GetName() const noexcept
	{
		return name;
	}

	[[nodiscard]] auto GetThreadID() const noexcept
	{
		return threadID;
	}

	[[nodiscard]] auto& GetThread() noexcept
	{
		return thread;
	}

	[[nodiscard]] auto& GetThread() const noexcept
	{
		return thread;
	}

	[[nodiscard]] auto GetStreamIndex() const noexcept
	{
		return streamIndex;
	}

	[[nodiscard]] auto GetLoopCount() const noexcept
	{
		return loopCount;
	}

	bool AttachProvider(TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept;
	bool DetachProvider(TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept;
	[[nodiscard]] bool IsProviderAttached(const TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept;

	void Start(TaskSystem& taskSys) noexcept;
	bool Update() noexcept;

	void RequestClose() noexcept
	{
		closeRequested.store(true, std::memory_order_release);
		cv.notify_all();
	}

	void SetNestedPumpAllowed(bool allowed) noexcept;

	void DispatchPostedTasks(MainThreadTaskQueue::TTaskFunc taskFunc, void* userData, uint8_t priority = 128) noexcept;
	size_t ProcessPostedTasks() noexcept;

	[[nodiscard]] bool HasPostedTasks() const noexcept;

	[[nodiscard]] bool IsCloseRequested() const noexcept
	{
		return closeRequested.load(std::memory_order_acquire);
	}

	void CloseDrivenStream() noexcept;
	std::size_t AbandonHeldWork() noexcept;

	[[nodiscard]] std::uint64_t GetDrivenPassCount() const noexcept
	{
		return drivenPassCount;
	}

	[[nodiscard]] bool IsClosed() const noexcept
	{
		return isClosed.load(std::memory_order_acquire);
	}

	std::uint64_t DrainForShutdown() noexcept;

	[[nodiscard]] std::size_t CountPendingItems() const noexcept;

	[[nodiscard]] bool IsDrivenByShutdownPump() const noexcept
	{
		return drivenByShutdownPump;
	}

	void SetDrivenByShutdownPump() noexcept
	{
		drivenByShutdownPump = true;
	}

	[[nodiscard]] std::size_t GetAgedOutWorkCount() const noexcept
	{
		return agedOutWorkCount.load(std::memory_order_relaxed);
	}

	[[nodiscard]] std::size_t GetAbandonedWorkNoticeCount() const noexcept
	{
		return abandonedWorkNoticeCount.load(std::memory_order_relaxed);
	}

	void WaitForWork(std::chrono::milliseconds patience) noexcept;
	void RunLoop() noexcept;

private:
	void Dequeue(std::optional<WorkItem>& outTask);

	std::optional<WorkItem> DrainProvidersLocked(StreamDrainPolicy::ELane lane) noexcept;

	void ReportReleasedTask(const WorkItem& task) const noexcept;
	void ReportAgedOutWorkItem(const WorkItem& item, std::chrono::nanoseconds age) const noexcept;

	void FireAbandonedNotice(const WorkItem& item) const noexcept;

	friend class TaskSystemTest;

	void ReportGeneralQueueRefusal() noexcept;
};
} // namespace hbe
