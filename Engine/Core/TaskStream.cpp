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
TaskStream::TaskStream()
	: laneProviders()
	, streamIndex(0)
	, loopCount(0)
	, allocator("None")
	, taskSystem(nullptr)
	, windowAdvanceRequested(false)
	, closeRequested(false)
	, isClosed(false)
	, drivenPassCount(0)
	, isPumping(false)
	, nestedPumpAllowed(false)
	, isDrainingForShutdown(false)
	, abandonedWorkNoticeCount(0)
	, agedOutWorkCount(0)
	, drivenByShutdownPump(false)
	, shutdownDrainDeadline(std::chrono::milliseconds(2000))
	, generalQueueRefusals(0)
	, laneWorkRefusals(0)
	, providerAsksWhileSpent(0)
{
	Assert(threadID == std::thread::id());
}

TaskStream::TaskStream(StaticString name, TStreamIndex streamIndex)
	: laneProviders()
	, name(name)
	, streamIndex(streamIndex)
	, loopCount(0)
	, allocator(name)
	, taskSystem(nullptr)
	, windowAdvanceRequested(false)
	, closeRequested(false)
	, isClosed(false)
	, drivenPassCount(0)
	, isPumping(false)
	, nestedPumpAllowed(false)
	, isDrainingForShutdown(false)
	, abandonedWorkNoticeCount(0)
	, agedOutWorkCount(0)
	, drivenByShutdownPump(false)
	, shutdownDrainDeadline(streamIndex == TaskSystem::BaseStreamIndex || streamIndex == TaskSystem::IOStreamIndex
									? std::chrono::milliseconds::zero()
									: std::chrono::milliseconds(2000))
	, generalQueueRefusals(0)
	, laneWorkRefusals(0)
	, providerAsksWhileSpent(0)
{
	auto log = Logger::Get(name);
	log.Out([name = name](auto& ls) { ls << name.c_str() << " is created."; });
}

namespace
{
constexpr size_t ProviderSlot(StreamDrainPolicy::ELane lane) noexcept
{
	return lane == StreamDrainPolicy::ELane::Fifo ? 0U : (lane == StreamDrainPolicy::ELane::Priority ? 1U : 2U);
}
} // namespace

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

void TaskStream::ConfigureRate(uint32_t fifoWeight, uint32_t priorityWeight) noexcept
{
	if (threadID != TThreadID{})
	{
		Assert(std::this_thread::get_id() == threadID, "Stream ", name,
			   " had its lane rate configured from another thread while it is running. The drain policy's weights are "
			   "unsynchronised fields this stream reads when it chooses a lane, so that is a data race.");
	}

	drainPolicy.ConfigureRate(fifoWeight, priorityWeight);
}

void TaskStream::SetMaxAge(std::chrono::nanoseconds maxAge) noexcept
{
	drainPolicy.SetMaxAge(maxAge);
}

void TaskStream::WakeUp() noexcept
{
	cv.notify_one();
}

void TaskStream::ConfigureBudget(std::chrono::duration<double> allowance) noexcept
{
	if (threadID != TThreadID{})
	{
		Assert(std::this_thread::get_id() == threadID, "Stream ", name,
			   " had its budget configured from another thread while it is running. The allowance is an "
			   "unsynchronised field this stream reads every pass, so that is a data race. Use RequestBudget, "
			   "which the stream applies to itself.");
	}

	budget.Configure(allowance);
	drainPolicy.ConfigureAllowance(allowance);
}

void TaskStream::RequestBudget(std::chrono::duration<double> allowance) noexcept
{
	budget.RequestAllowance(allowance);
}

void TaskStream::RequestWindowAdvance() noexcept
{
	windowAdvanceRequested.store(true, std::memory_order_relaxed);
}

void TaskStream::Join() noexcept
{
	thread.join();
}

void TaskStream::Start(TaskSystem& taskSys) noexcept
{
	taskSystem = &taskSys;

	if (streamIndex == TaskSystem::BaseStreamIndex)
	{
		threadID = std::this_thread::get_id();

		return;
	}

	if (streamIndex == TaskSystem::IOStreamIndex)
	{
		return;
	}

	auto func = [this]() { RunLoop(); };

	thread = std::thread(func);
	OS::SetThreadPriority(thread, 0);
}

void TaskStream::RequestClose() noexcept
{
	closeRequested.store(true, std::memory_order_release);
	cv.notify_all();
}

void TaskStream::SetNestedPumpAllowed(bool allowed) noexcept
{
	nestedPumpAllowed = allowed;
}

void TaskStream::DispatchPostedTasks(MainThreadTaskQueue::TTaskFunc taskFunc, void* userData,
									 const uint8_t priority) noexcept
{
	postedTasks.Enqueue(taskFunc, userData, priority);
}

void TaskStream::CloseDrivenStream() noexcept
{
	RequestClose();

	if (isClosed.exchange(true, std::memory_order_acq_rel))
	{
		return;
	}

	if (const auto abandoned = AbandonHeldWork(); abandoned > 0)
	{
		Logger::Get(name).OutWarning([name = name, abandoned](auto& ls)
		{
			ls << name.c_str() << " is closing with " << abandoned
			   << " item(s) still held. They are abandoned, not requeued, and every requestor that asked to be told "
				  "has been told, on this thread, as each one was dropped."
			   << " The count is not the report: an item discarded without a word is a promise cancelled silently.";
		});
	}

	Logger::Get(name).Out([name = name, passes = drivenPassCount](auto& ls)
	{ ls << name.c_str() << " closed after " << passes << " driven pass(es)."; });
}

void TaskStream::SetDrivenByShutdownPump() noexcept
{
	drivenByShutdownPump = true;
}

void TaskStream::WaitForWork(std::chrono::milliseconds patience) noexcept
{
	std::unique_lock lock(queueLock);
	cv.wait_for(lock, patience);
}

void TaskStream::RunLoop() noexcept
{
	AllocatorScope scope(allocator);

	TaskSystem::SetThreadName(name);
	TaskSystem::SetStreamIndex(streamIndex);

	const auto log = Logger::Get(name);
	log.Out([name = name](auto& ls) { ls << name.c_str() << " has begun."; });

	threadID = std::this_thread::get_id();

	while (likely(!closeRequested.load(std::memory_order_acquire)))
	{
		if (!Update())
		{
			WaitForWork(std::chrono::milliseconds(10));
		}
	}

	const auto drainPasses = DrainForShutdown();

	log.Out([name = name, drainPasses](auto& ls)
	{ ls << name.c_str() << " closed after draining " << drainPasses << " pass(es)."; });

	isClosed.store(true, std::memory_order_release);
}

StaticString TaskStream::GetName() const noexcept
{
	return name;
}

std::thread::id TaskStream::GetThreadID() const noexcept
{
	return threadID;
}

std::thread& TaskStream::GetThread() noexcept
{
	return thread;
}

const std::thread& TaskStream::GetThread() const noexcept
{
	return thread;
}

TStreamIndex TaskStream::GetStreamIndex() const noexcept
{
	return streamIndex;
}

std::uint64_t TaskStream::GetLoopCount() const noexcept
{
	return loopCount;
}

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

bool TaskStream::Update() noexcept
{
	if (threadID == TThreadID{})
	{
		threadID = std::this_thread::get_id();
	}

	Assert(!isPumping || nestedPumpAllowed, "Stream ", name,
		   " was pumped while it was already pumping; only a designated wait point may nest a pump.");

	isPumping = true;

	struct PumpGuard
	{
		bool& flag;

		~PumpGuard()
		{
			flag = false;
		}
	} pumpGuard{isPumping};

	++drivenPassCount;

	postedTasks.ProcessTasks();

	if (closeRequested.load(std::memory_order_acquire))
	{
		return false;
	}

	const TStreamIndex previousStreamIndex = TaskSystem::GetCurrentStreamIndex();
	TaskSystem::SetStreamIndex(streamIndex);
	AllocatorScope scope(allocator);

	auto restore = [&]() { TaskSystem::SetStreamIndex(previousStreamIndex); };

	static ConfigParam<float, true> thresholdDuration(
			"TaskStreamDurationThreshold", "Print a warning log if it detects slower task. (seconds)", 0.16f);

	TaskSystem& taskSys = *taskSystem;
	++loopCount;

	if (windowAdvanceRequested.exchange(false, std::memory_order_relaxed))
	{
		budget.Reset();
		drainPolicy.EndRound();
	}

	if (auto applied = budget.ApplyRequestedAllowance(); applied.has_value())
	{
		drainPolicy.ConfigureAllowance(*applied);
	}

	if (streamIndex == TaskSystem::BaseStreamIndex)
	{
		taskSys.RunBudgetWindowPass();
	}

	std::optional<WorkItem> workItem;
	StreamDrainPolicy::ELane lane = StreamDrainPolicy::ELane::None;
	{
		std::unique_lock lock(queueLock);

		priorityQueue.Remove([](const WorkItem& task) { return task.HasFinished(); });
		priorityQueue.PushRange(readdingPriority);
		readdingPriority.clear();

		for (auto& task : readdingFifo)
		{
			fifoQueue.PushBack(task);
		}

		readdingFifo.clear();

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

		if (lane == StreamDrainPolicy::ELane::None && (!fifoQueue.IsEmpty() || !priorityQueue.IsEmpty()))
		{
			laneWorkRefusals.fetch_add(1, std::memory_order_relaxed);
		}

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
			for (const auto probe : {StreamDrainPolicy::ELane::Fifo, StreamDrainPolicy::ELane::Priority})
			{
				const bool laneIsEmpty =
						probe == StreamDrainPolicy::ELane::Fifo ? fifoQueue.IsEmpty() : priorityQueue.IsEmpty();
				if (!laneIsEmpty || isDrainingForShutdown || !budget.CanTakeWork())
				{
					continue;
				}

				Assert(budget.CanTakeWork(), "Stream ", name,
					   " was asked for provider work while its allowance was spent. The drain gate is what keeps a "
					   "spent stream "
					   "from manufacturing work, so a provider ask here means that gate stopped consulting the "
					   "budget.");

				if (!budget.CanTakeWork())
				{
					providerAsksWhileSpent.fetch_add(1, std::memory_order_relaxed);
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
		restore();

		return false;
	}

	auto* task = taskSys.FindTask(workItem->taskID);
	if (task == nullptr)
	{
		ReportReleasedTask(*workItem);
		FireAbandonedNotice(*workItem);
		restore();

		return true;
	}

	if (const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(time::ElapsedSinceEngineEpoch());
		drainPolicy.IsOverAge(workItem->offerTime, now))
	{
		agedOutWorkCount.fetch_add(1, std::memory_order_relaxed);
		ReportAgedOutWorkItem(*workItem, now - workItem->offerTime);
		FireAbandonedNotice(*workItem);
		restore();

		return true;
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
		Logger::Get(name).OutWarning([dt = deltaTime](auto& ls) { ls << "Slow DeltaTime = " << dt; });
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

	restore();

	return true;
}

bool TaskStream::IsCloseRequested() const noexcept
{
	return closeRequested.load(std::memory_order_acquire);
}

bool TaskStream::IsClosed() const noexcept
{
	return isClosed.load(std::memory_order_acquire);
}

std::uint64_t TaskStream::DrainForShutdown() noexcept
{
	isDrainingForShutdown = true;
	const auto deadline = std::chrono::steady_clock::now() + shutdownDrainDeadline;

	std::uint64_t passes = 0;

	while (Update() && std::chrono::steady_clock::now() < deadline)
	{
		++passes;
	}

	isDrainingForShutdown = false;

	if (const auto abandoned = AbandonHeldWork(); abandoned > 0)
	{
		Logger::Get(name).OutWarning([name = name, abandoned](auto& ls)
		{
			ls << name.c_str() << " is closing with " << abandoned
			   << " item(s) still held. They are abandoned, not requeued: a task that cannot finish inside the drain"
			   << " deadline is a defect in that task, and the shutdown must report it and proceed - reporting it "
				  "means "
				  "firing the notice as well, not only naming a number."
			   << " Abandoned here are " << abandoned << ".";
		});
	}

	return passes;
}

std::size_t TaskStream::AbandonHeldWork() noexcept
{
	std::size_t abandoned = 0;

	while (!priorityQueue.IsEmpty())
	{
		if (const auto item = priorityQueue.Pop(); item.has_value())
		{
			FireAbandonedNotice(*item);
			++abandoned;
		}
	}
	while (!fifoQueue.IsEmpty())
	{
		const WorkItem item = fifoQueue.Front();
		fifoQueue.PopFront();
		FireAbandonedNotice(item);
		++abandoned;
	}

	abandonedWorkNoticeCount.fetch_add(abandoned, std::memory_order_relaxed);

	return abandoned;
}

bool TaskStream::HasPostedTasks() const noexcept
{
	return postedTasks.HasPendingTasks();
}

std::chrono::nanoseconds TaskStream::GetMaxAge() const noexcept
{
	return drainPolicy.GetMaxAge();
}

uint32_t TaskStream::GetFifoWeight() const noexcept
{
	return drainPolicy.GetFifoWeight();
}

uint32_t TaskStream::GetPriorityWeight() const noexcept
{
	return drainPolicy.GetPriorityWeight();
}

bool TaskStream::MayTakeNewWork() const noexcept
{
	return budget.CanTakeWork();
}

std::chrono::nanoseconds TaskStream::GetAccumulatedCPUTime() const noexcept
{
	return budget.GetAccumulated();
}

std::size_t TaskStream::CountPendingItems() const noexcept
{
	return fifoQueue.Size() + priorityQueue.Size();
}

std::uint64_t TaskStream::GetDrivenPassCount() const noexcept
{
	return drivenPassCount;
}

bool TaskStream::IsDrivenByShutdownPump() const noexcept
{
	return drivenByShutdownPump;
}

std::size_t TaskStream::GetAgedOutWorkCount() const noexcept
{
	return agedOutWorkCount.load(std::memory_order_relaxed);
}

std::size_t TaskStream::GetAbandonedWorkNoticeCount() const noexcept
{
	return abandonedWorkNoticeCount.load(std::memory_order_relaxed);
}

unsigned TaskStream::GetLaneWorkRefusalCount() const noexcept
{
	return laneWorkRefusals.load(std::memory_order_relaxed);
}

unsigned TaskStream::GetProviderAskWhileSpentCount() const noexcept
{
	return providerAsksWhileSpent.load(std::memory_order_relaxed);
}

unsigned TaskStream::GetGeneralQueueRefusalCount() const noexcept
{
	return generalQueueRefusals.load(std::memory_order_relaxed);
}

void TaskStream::FireAbandonedNotice(const WorkItem& item) const noexcept
{
	if (item.abandonedNotice != nullptr)
	{
		item.abandonedNotice(item.taskID, item.abandonedUserData);
	}
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

void TaskStream::Dequeue(std::optional<WorkItem>& outTask)
{
	std::scoped_lock<std::mutex> lock(queueLock);

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

void TaskStream::ReportReleasedTask(const WorkItem& item) const noexcept
{
	auto log = Logger::Get(name);
	log.OutWarning([name = name, index = item.taskID.index, generation = item.taskID.generation](auto& ls)
	{
		ls << name.c_str() << " dropped a work item, record " << index << " generation " << generation
		   << ", because that task had already been released. Nothing was run.";
	});
}

void TaskStream::ReportAgedOutWorkItem(const WorkItem& item, std::chrono::nanoseconds age) const noexcept
{
	auto log = Logger::Get(name);
	log.OutWarning([name = name, index = item.taskID.index, generation = item.taskID.generation, age](auto& ls)
	{
		ls << name.c_str() << " dropped a work item, record " << index << " generation " << generation << ", aged "
		   << age.count() << "ns, older than this stream's max age. Nothing was run.";
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
} // namespace hbe
