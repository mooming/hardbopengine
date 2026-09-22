// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TaskStream.h"

#include <thread>

#include "Config/ConfigParam.h"
#include "Core/Debug.h"
#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "OSAL/OSThread.h"
#include "ScopedTime.h"
#include "TaskProvider.h"
#include "TaskSystem.h"

namespace hbe
{

TaskStream::TaskQueueItem::TaskQueueItem(uint8_t priority, const WorkItem& task)
	: priority(priority)
	, task(task)
	, duration(0)
{
}

bool TaskStream::TaskQueueItem::operator<(const TaskQueueItem& other) const
{
	return priority < other.priority;
}

TaskStream::TaskStream()
	: streamIndex(0)
	, loopCount(0)
	, allocator("None")
{
	Assert(threadID == std::thread::id());
}

TaskStream::TaskStream(StaticString name, TStreamIndex streamIndex)
	: name(name)
	, streamIndex(streamIndex)
	, loopCount(0)
	, allocator(name)
{
	auto log = Logger::Get(name);
	log.Out([name = name](auto& ls) { ls << name.c_str() << " is created."; });
}

void TaskStream::EnqueueFifo(const WorkItem& task) noexcept
{
	std::scoped_lock<std::mutex> lock(queueLock);
	fifoQueue.PushBack(task);
	cv.notify_one();
}

void TaskStream::EnqueuePriority(const WorkItem& task) noexcept
{
	std::scoped_lock<std::mutex> lock(queueLock);
	priorityQueue.Push(task);
	cv.notify_one();
}

namespace
{
/// @brief Which list serves a lane, or an out-of-range index when the lane is `None`.
constexpr size_t ProviderSlot(StreamDrainPolicy::ELane lane) noexcept
{
	return lane == StreamDrainPolicy::ELane::Fifo ? 0U : (lane == StreamDrainPolicy::ELane::Priority ? 1U : 2U);
}
} // namespace

bool TaskStream::AttachProvider(TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept
{
	const size_t slot = ProviderSlot(lane);
	if (slot >= laneProviders.size())
	{
		return false;
	}

	std::scoped_lock<std::mutex> lock(queueLock);
	auto& list = laneProviders[slot];

	for (TIndex index = 0; index < list.count; ++index)
	{
		if (list.items[static_cast<size_t>(index)] == &provider)
		{
			return false;
		}
	}

	if (list.count >= MaxProvidersPerLane)
	{
		return false;
	}

	list.items[static_cast<size_t>(list.count)] = &provider;
	++list.count;
	cv.notify_one();
	return true;
}

bool TaskStream::DetachProvider(TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept
{
	const size_t slot = ProviderSlot(lane);
	if (slot >= laneProviders.size())
	{
		return false;
	}

	std::scoped_lock<std::mutex> lock(queueLock);
	auto& list = laneProviders[slot];

	for (TIndex index = 0; index < list.count; ++index)
	{
		if (list.items[static_cast<size_t>(index)] != &provider)
		{
			continue;
		}

		for (TIndex shift = index; shift + 1 < list.count; ++shift)
		{
			list.items[static_cast<size_t>(shift)] = list.items[static_cast<size_t>(shift + 1)];
		}

		list.items[static_cast<size_t>(list.count - 1)] = nullptr;
		--list.count;
		return true;
	}

	return false;
}

bool TaskStream::IsProviderAttached(const TaskProvider& provider, StreamDrainPolicy::ELane lane) noexcept
{
	const size_t slot = ProviderSlot(lane);
	if (slot >= laneProviders.size())
	{
		return false;
	}

	std::scoped_lock<std::mutex> lock(queueLock);
	const auto& list = laneProviders[slot];

	for (TIndex index = 0; index < list.count; ++index)
	{
		if (list.items[static_cast<size_t>(index)] == &provider)
		{
			return true;
		}
	}

	return false;
}

std::optional<WorkItem> TaskStream::DrainProvidersLocked(StreamDrainPolicy::ELane lane) noexcept
{
	const size_t slot = ProviderSlot(lane);
	if (slot >= laneProviders.size())
	{
		return std::nullopt;
	}

	const auto& list = laneProviders[slot];
	if (list.count == 0)
	{
		return std::nullopt;
	}

	const TaskProduceContext context = TaskProduceContext::ForStream(streamIndex);

	for (TIndex index = 0; index < list.count; ++index)
	{
		if (auto produced = list.items[static_cast<size_t>(index)]->Produce(context); produced.has_value())
		{
			return produced;
		}
	}

	return std::nullopt;
}

void TaskStream::ReportReleasedTask(const WorkItem& item) const noexcept
{
	auto log = Logger::Get(name);
	log.OutWarning([name = name, index = item.taskID.index, generation = item.taskID.generation](auto& ls)
	{
		ls << name.c_str() << " dropped a work item, record " << index << " generation " << generation
		   << ", because that task had already been released. Nothing was run.";
	});
}

void TaskStream::ReportGeneralQueueRefusal() noexcept
{
	if (generalQueueRefusals.fetch_add(1, std::memory_order_relaxed) != 0)
	{
		return;
	}

	auto log = Logger::Get(name);
	log.OutWarning([name = name, allowance = budget.GetAllowance()](auto& ls)
	{
		ls << name.c_str() << " has spent its allowance of "
		   << std::chrono::duration_cast<std::chrono::microseconds>(allowance).count()
		   << " us and is leaving tasks in the general queue until the budget window advances again.";
	});
}

void TaskStream::ConfigureRate(uint32_t fifoWeight, uint32_t priorityWeight) noexcept
{
	drainPolicy.ConfigureRate(fifoWeight, priorityWeight);
}

void TaskStream::Dequeue(std::optional<WorkItem>& outTask)
{
	std::scoped_lock<std::mutex> lock(queueLock);

	// Serves work whenever there is any, without consulting the drain policy: the rate and the budget gate
	// the stream's own loop, and gating here would let a caller's refill stall on an exhausted allowance.
	if (!priorityQueue.IsEmpty())
	{
		outTask = priorityQueue.Pop();
		drainPolicy.CommitTake(StreamDrainPolicy::ELane::Priority);
		return;
	}

	if (!fifoQueue.IsEmpty())
	{
		outTask = fifoQueue.Front();
		fifoQueue.PopFront();
		drainPolicy.CommitTake(StreamDrainPolicy::ELane::Fifo);
		return;
	}

	outTask.reset();
}

void TaskStream::WakeUp() noexcept
{
	cv.notify_one();
}

void TaskStream::ConfigureBudget(std::chrono::duration<double> allowance) noexcept
{
	budget.Configure(allowance);
	drainPolicy.ConfigureAllowance(allowance);
}

bool TaskStream::MayTakeNewWork() const noexcept
{
	return budget.CanTakeWork();
}

std::chrono::nanoseconds TaskStream::GetAccumulatedCPUTime() const noexcept
{
	return budget.GetAccumulated();
}

void TaskStream::Start(TaskSystem& taskSys) noexcept
{
	auto func = [this]() { RunLoop(); };

	thread = std::thread(func);
	OS::SetThreadPriority(thread, 0);
}

void TaskStream::RunLoop() noexcept
{
	AllocatorScope scope(allocator);

	TaskSystem::SetThreadName(name);
	TaskSystem::SetStreamIndex(streamIndex);

	const auto log = Logger::Get(name);
	log.Out([name = name](auto& ls) { ls << name.c_str() << " has begun."; });

	threadID = std::this_thread::get_id();

	static ConfigParam<float, true> thresholdDuration(
			"TaskStreamDurationThreshold", "Print a warning log if it detects slower task. (seconds)", 0.16f);

	auto& engine = Engine::Get();
	auto& taskSys = engine.GetTaskSystem();

	HVector<WorkItem> readdingFifo;
	HVector<WorkItem> readdingPriority;

	for (; likely(taskSys.IsRunning()); ++loopCount)
	{
		if (windowAdvanceRequested.exchange(false, std::memory_order_relaxed))
		{
			budget.Reset();
			drainPolicy.EndRound();
		}

		if (streamIndex == TaskSystem::BaseStreamIndex)
		{
			taskSys.RunBudgetWindowPass();
		}

		std::optional<WorkItem> workItem;
		StreamDrainPolicy::ELane lane = StreamDrainPolicy::ELane::None;

		{
			std::unique_lock lock(queueLock);

			// A task that finished elsewhere is released from its lane and put back on that same lane, so the
			// sweep and the re-add are per lane and no task changes lane on the way.
			priorityQueue.Remove([](const WorkItem& task) { return task.HasFinished(); });
			priorityQueue.PushRange(readdingPriority);
			readdingPriority.clear();

			for (auto& task : readdingFifo)
			{
				fifoQueue.PushBack(task);
			}

			readdingFifo.clear();

			// Rotating the lane drops finished tasks in place and preserves arrival order for the rest, which
			// is the same O(n)-per-loop price BoundedPriorityQueue::Remove already pays.
			const size_t fifoCount = fifoQueue.Size();
			for (size_t i = 0; i < fifoCount; ++i)
			{
				auto entry = fifoQueue.Front();
				fifoQueue.PopFront();

				if (!entry.HasFinished())
				{
					fifoQueue.PushBack(entry);
				}
			}

			lane = drainPolicy.ChooseLane(!fifoQueue.IsEmpty(), !priorityQueue.IsEmpty());
			switch (lane)
			{
				case StreamDrainPolicy::ELane::Fifo:
					workItem = fifoQueue.Front();
					fifoQueue.PopFront();
					drainPolicy.CommitTake(lane);
					break;
				case StreamDrainPolicy::ELane::Priority:
					workItem = priorityQueue.Pop();
					drainPolicy.CommitTake(lane);
					break;
				case StreamDrainPolicy::ELane::None:
					break;
			}

			if (!workItem.has_value())
			{
				// Both lanes offered nothing, so ask whoever attached to them - the lane that is empty is the one that
				// asks, and a spent allowance means no asking at all (R39).
				for (const auto probe : {StreamDrainPolicy::ELane::Fifo, StreamDrainPolicy::ELane::Priority})
				{
					const bool laneIsEmpty =
							probe == StreamDrainPolicy::ELane::Fifo ? fifoQueue.IsEmpty() : priorityQueue.IsEmpty();
					if (!laneIsEmpty || !budget.CanTakeWork())
					{
						continue;
					}

					if (auto produced = DrainProvidersLocked(probe); produced.has_value())
					{
						workItem = produced;
						lane = probe;
						drainPolicy.CommitTake(probe);
						break;
					}
				}
			}

			if (!workItem.has_value())
			{
				lane = StreamDrainPolicy::ELane::None;
			}
		}

		if (!workItem.has_value())
		{
			if (budget.CanTakeWork())
			{
				taskSys.Dequeue(workItem);
			}
			else
			{
				ReportGeneralQueueRefusal();
			}
		}

		if (!workItem.has_value())
		{
			// Wait for a signal for waitPeriod
			std::unique_lock lock(queueLock);
			constexpr std::chrono::milliseconds waitPeriod(10);
			cv.wait_for(lock, waitPeriod);

			continue;
		}

		auto* task = taskSys.FindTask(workItem->taskID);
		if (task == nullptr)
		{
			ReportReleasedTask(*workItem);
			continue;
		}

		time::TDuration duration;
		const bool chargingBudget = budget.GetAllowance().count() > 0.0;
		const auto chargedBefore = chargingBudget ? budget.GetAccumulated() : std::chrono::nanoseconds::zero();
		if (chargingBudget)
		{
			budget.BeginTask();
		}

		{
			time::ScopedTime timer(duration);
			if (workItem->Run(*task))
			{
				taskSys.DispatchSuccessor(task->GetID());
			}
		}

		if (chargingBudget)
		{
			budget.EndTask();

			const auto spent = budget.GetAccumulated() - chargedBefore;
			if (lane == StreamDrainPolicy::ELane::Priority)
			{
				drainPolicy.ChargePriority(spent);
			}
			else
			{
				drainPolicy.ChargeFifo(spent);
			}
		}

		const float deltaTime = time::ToFloat(duration);
		if (deltaTime > thresholdDuration.Get())
		{
			log.OutWarning([dt = deltaTime](auto& ls) { ls << "Slow DeltaTime = " << dt; });
		}

		if (!workItem->HasFinished())
		{
			if (lane == StreamDrainPolicy::ELane::Priority)
			{
				readdingPriority.push_back(*workItem);
			}
			else
			{
				readdingFifo.push_back(*workItem);
			}
		}
	}

	log.Out([name = name](auto& ls) { ls << name.c_str() << " has been terminated."; });
}
} // namespace hbe
