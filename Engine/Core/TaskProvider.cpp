// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TaskProvider.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "Core/Debug.h"
#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "TaskSystem.h"


namespace hbe
{
TaskProduceContext TaskProduceContext::ForStream(TStreamIndex streamIndex) noexcept
{
	return TaskProduceContext{streamIndex, std::chrono::steady_clock::now()};
}

TaskHandle::TaskHandle(TaskProvider& target) noexcept
	: provider(&target)
{
}

bool TaskHandle::IsStopRequested() const noexcept
{
	return provider != nullptr && provider->IsStopRequested();
}

void TaskHandle::RequestStop() const noexcept
{
	if (provider != nullptr)
	{
		provider->Stop();
	}
}

TaskProvider::TaskProvider(StaticString providerName, TaskSystem& targetSystem) noexcept
	: name(providerName)
	, taskSystem(targetSystem)
{
}

TaskProvider::~TaskProvider()
{
	Assert(registeredCount == 0, "TaskProvider ", name, " was destroyed while it still fed ", registeredCount,
		   " live stream(s). A stream holds this object's pointer, so destroy it only after DetachFrom or DetachAll.");
}

void TaskProvider::AttachTo(TStreamIndex stream, StreamDrainPolicy::ELane lane) noexcept
{
	const std::uint8_t laneBit = LaneBit(lane);

	if (laneBit == 0)
	{
		auto log = Logger::Get(name);
		log.OutError([name = name, stream](auto& ls)
		{
			ls << "TaskProvider " << name.c_str() << " was attached to stream " << stream
			   << " with no lane, so nothing was attached. An attachment with no lane sits in a slot that no"
			   << " drain ever reads.";
		});

		return;
	}

	for (TStreamIndex index = 0; index < attachedCount; ++index)
	{
		if (attached[static_cast<size_t>(index)] != stream)
		{
			continue;
		}

		const bool alreadyOnLane = (lanes[static_cast<size_t>(index)] & laneBit) != 0;
		lanes[static_cast<size_t>(index)] |= laneBit;

		if (!alreadyOnLane)
		{
			RegisterOnStream(stream, lane);
		}

		return;
	}

	if (attachedCount >= MaxAttachedStreams)
	{
		Assert(false, "TaskProvider ", name, " is already attached to its maximum of ", MaxAttachedStreams,
			   " streams; this attachment was dropped");

		auto log = Logger::Get(name);
		const auto dropped = attachedCount;
		log.OutError([name = name, stream, laneBit, dropped](auto& ls)
		{
			ls << "TaskProvider " << name.c_str() << " could not attach to stream " << stream << " on lane "
			   << static_cast<unsigned int>(laneBit) << ": it already feeds " << dropped
			   << " streams. The attachment was dropped, and the stream will never ask this provider for work.";
		});

		return;
	}

	attached[static_cast<size_t>(attachedCount)] = stream;
	lanes[static_cast<size_t>(attachedCount)] = laneBit;
	++attachedCount;

	RegisterOnStream(stream, lane);
}

void TaskProvider::DetachFrom(TStreamIndex stream) noexcept
{
	for (TStreamIndex index = 0; index < attachedCount; ++index)
	{
		if (attached[static_cast<size_t>(index)] != stream)
		{
			continue;
		}

		const std::uint8_t held = lanes[static_cast<size_t>(index)];

		if ((held & LaneBitFifo) != 0)
		{
			UnregisterFromStream(stream, StreamDrainPolicy::ELane::Fifo);
		}

		if ((held & LaneBitPriority) != 0)
		{
			UnregisterFromStream(stream, StreamDrainPolicy::ELane::Priority);
		}

		for (TStreamIndex shift = index; shift + 1 < attachedCount; ++shift)
		{
			attached[static_cast<size_t>(shift)] = attached[static_cast<size_t>(shift + 1)];
			lanes[static_cast<size_t>(shift)] = lanes[static_cast<size_t>(shift + 1)];
		}

		attached[static_cast<size_t>(attachedCount - 1)] = 0;
		lanes[static_cast<size_t>(attachedCount - 1)] = 0;
		--attachedCount;

		return;
	}
}

void TaskProvider::DetachAll() noexcept
{
	for (TStreamIndex index = attachedCount - 1; index >= 0; --index)
	{
		DetachFrom(attached[static_cast<size_t>(index)]);
	}
}

void TaskProvider::RegisterOnStream(TStreamIndex stream, StreamDrainPolicy::ELane lane) noexcept
{
	if (!taskSystem.HasStream(stream))
	{
		auto log = Logger::Get(name);
		log.OutWarning([name = name, stream](auto& ls)
		{
			ls << "TaskProvider " << name.c_str() << " is attached to stream " << stream
			   << ", which this engine does not have. The attachment is recorded and nothing will ever ask this"
			   << " provider for work.";
		});

		return;
	}

	if (!taskSystem.GetStream(stream).AttachProvider(*this, lane))
	{
		auto log = Logger::Get(name);
		log.OutError([name = name, stream](auto& ls)
		{
			ls << "TaskProvider " << name.c_str() << " could not register on stream " << stream
			   << ". The lane list already holds this provider or is full, so the drain will not ask it.";
		});

		return;
	}

	++registeredCount;
}

void TaskProvider::UnregisterFromStream(TStreamIndex stream, StreamDrainPolicy::ELane lane) noexcept
{
	if (!taskSystem.HasStream(stream))
	{
		return;
	}

	if (taskSystem.GetStream(stream).DetachProvider(*this, lane))
	{
		--registeredCount;
	}
}

TStreamIndex TaskProvider::GetAttachedStream(TStreamIndex index) const noexcept
{
	Assert(index >= 0 && index < attachedCount, "TaskProvider ", name, " asked for attachment ", index, " of ",
		   attachedCount);
	if (index < 0 || index >= attachedCount)
	{
		return 0;
	}

	return attached[static_cast<size_t>(index)];
}

std::uint8_t TaskProvider::GetAttachedLanes(TStreamIndex stream) const noexcept
{
	for (TStreamIndex index = 0; index < attachedCount; ++index)
	{
		if (attached[static_cast<size_t>(index)] == stream)
		{
			return lanes[static_cast<size_t>(index)];
		}
	}

	return 0;
}

bool TaskProvider::IsAttachedTo(TStreamIndex stream) const noexcept
{
	for (TStreamIndex index = 0; index < attachedCount; ++index)
	{
		if (attached[static_cast<size_t>(index)] == stream)
		{
			return true;
		}
	}

	return false;
}

WorkItem TaskProvider::MakeWholeItem(Task& task, const uint8_t priority) noexcept
{
	task.ReserveSubTasks(1);

	return task.GenerateSubTask(0, 1, priority);
}

void TaskProvider::Stop() noexcept
{
	stopRequested.store(true, std::memory_order_release);
}

TaskHandle TaskProvider::GetHandle() noexcept
{
	return TaskHandle{*this};
}
} // namespace hbe

#ifdef __UNIT_TEST__
namespace
{
class RecordingProvider final : public hbe::TaskProvider
{
public:
	using TaskProvider::TaskProvider;

	int produceCalls = 0;

	hbe::TStreamIndex lastStream = -1;

	hbe::time::TEngineTimePoint lastNow{};

	bool produceResult = true;

	std::optional<hbe::WorkItem> itemToHand{};

	~RecordingProvider() override
	{
		DetachAll();
	}

	std::optional<hbe::WorkItem> Produce(const hbe::TaskProduceContext& context) noexcept override
	{
		++produceCalls;
		lastStream = context.stream;
		lastNow = context.now;

		if (!produceResult)
		{
			return std::nullopt;
		}

		return itemToHand;
	}
};

struct DrainObservation
{
public:
	std::atomic<unsigned> runs{0};

	std::atomic<int> ranOnStream{-1};
};

class DrainingProvider final : public hbe::TaskProvider
{
public:
	bool produceResult = true;

	bool releaseImmediately = false;

private:
	static constexpr int MaxItems = 4;

	hbe::TaskSystem& system;

	DrainObservation& observation;

	std::array<hbe::TaskID, MaxItems> ids{hbe::TaskID{}, hbe::TaskID{}, hbe::TaskID{}, hbe::TaskID{}};

	int asks = 0;

	int handed = 0;

public:
	DrainingProvider(hbe::StaticString name, hbe::TaskSystem& system, DrainObservation& target) noexcept
		: TaskProvider(name, system)
		, system(system)
		, observation(target)
	{
	}

	~DrainingProvider() override
	{
		DetachAll();
	}

	std::optional<hbe::WorkItem> Produce(const hbe::TaskProduceContext& context) noexcept override
	{
		++asks;

		if (handed >= MaxItems || !produceResult)
		{
			return std::nullopt;
		}

		const hbe::TaskID id = system.CreateTask("DrainTick", CountOnce, &observation);
		if (id.IsNull())
		{
			return std::nullopt;
		}

		ids[handed] = id;
		++handed;

		hbe::Task* task = system.FindTask(id);
		if (task == nullptr)
		{
			return std::nullopt;
		}

		const auto item = MakeWholeItem(*task);

		if (releaseImmediately)
		{
			system.ReleaseTask(id);
		}

		return item;
	}

	[[nodiscard]] int GetAsks() const noexcept
	{
		return asks;
	}

	[[nodiscard]] int GetHanded() const noexcept
	{
		return handed;
	}

	void ReleaseItems() noexcept
	{
		for (int index = 0; index < handed; ++index)
		{
			system.ReleaseTask(ids[index]);
		}

		handed = 0;
	}

private:
	static std::size_t CountOnce(void* userData, std::size_t startIndex, std::size_t endIndex) noexcept
	{
		auto& observation = *static_cast<DrainObservation*>(userData);
		observation.runs.fetch_add(1, std::memory_order_relaxed);
		observation.ranOnStream.store(static_cast<int>(hbe::TaskSystem::GetCurrentStreamIndex()),
									  std::memory_order_relaxed);

		return endIndex - startIndex;
	}
};

class BlockingProvider final : public hbe::TaskProvider
{
public:
	std::atomic<bool> inside{false};

	std::atomic<bool> release{false};

	BlockingProvider(hbe::StaticString name, hbe::TaskSystem& system) noexcept
		: TaskProvider(name, system)
	{
	}

	~BlockingProvider() override
	{
		DetachAll();
	}

	std::optional<hbe::WorkItem> Produce(const hbe::TaskProduceContext& context) noexcept override
	{
		inside.store(true, std::memory_order_release);

		for (int spin = 0; spin < 5000 && !release.load(std::memory_order_acquire); ++spin)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		inside.store(false, std::memory_order_release);

		return std::nullopt;
	}
};

bool WaitFor(const std::function<bool()>& done, int attempts) noexcept
{
	for (int attempt = 0; attempt < attempts; ++attempt)
	{
		if (done())
		{
			return true;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	return done();
}
} // namespace

void hbe::TaskProviderTest::Prepare()
{
	AddTest("Produce is handed the stream and a live clock reading", [this](auto& ls)
	{
		RecordingProvider provider("ProviderContextProbe", System());

		const auto before = std::chrono::steady_clock::now();
		const auto context = TaskProduceContext::ForStream(3);
		provider.Produce(context);
		const auto after = std::chrono::steady_clock::now();

		if (provider.lastStream != 3)
		{
			ls << "Produce was told it was producing into stream " << provider.lastStream << ", not stream 3." << lferr;
		}

		if (provider.lastNow < before || provider.lastNow > after)
		{
			ls << "The context's clock reading was taken outside the call, so it is not a reading of this drain."
			   << lferr;
		}

		if (provider.lastNow < time::GetEngineEpoch())
		{
			ls << "The context's clock reading predates the engine epoch, which means it was not taken in the epoch's "
				  "time base."
			   << lferr;
		}
	});

	AddTest("A provider's item is run by the stream that drained it", [this](auto& ls)
	{
		auto& taskSystem = System();
		const auto worker = hbe::TaskSystem::GetIOTaskStreamIndex() + 1;

		if (!taskSystem.HasStream(worker))
		{
			ls << "This engine has no worker stream past the IO stream, so the drain cannot be observed at all."
			   << lferr;

			return;
		}

		DrainObservation observation;
		DrainingProvider provider("DrainDeliveryProbe", taskSystem, observation);
		provider.AttachTo(worker, StreamDrainPolicy::ELane::Fifo);

		const bool drained = WaitFor([&]() { return observation.runs.load(std::memory_order_relaxed) >= 4; }, 500);

		const unsigned runs = observation.runs.load(std::memory_order_relaxed);
		const int ranOn = observation.ranOnStream.load(std::memory_order_relaxed);

		provider.DetachAll();

		if (!drained)
		{
			ls << "The stream never ran anything the provider handed over: " << provider.GetAsks() << " asks produced "
			   << provider.GetHanded() << " item(s) and " << runs
			   << " ran. Asking is not delivering - this test exists because a drain that never asks would otherwise"
			   << " fail silently." << lferr;
		}

		if (provider.GetHanded() > 0 && runs == 0)
		{
			ls << "The provider handed over " << provider.GetHanded()
			   << " item(s) and none ran, so the drain takes items but the stream does not execute them." << lferr;
		}

		if (runs > 0 && ranOn != static_cast<int>(worker))
		{
			ls << "The work ran on stream " << ranOn << " instead of " << worker
			   << ", which is the stream the provider attached to. Routing by lane is the whole point of naming a lane"
			   << " (R35)." << lferr;
		}

		if (taskSystem.GetStream(worker).IsProviderAttached(provider, StreamDrainPolicy::ELane::Fifo))
		{
			ls << "DetachAll left the provider registered on the stream, so the stream still holds a pointer to an"
			   << " object about to die." << lferr;
		}

		provider.ReleaseItems();
	});

	AddTest("A provider with nothing to hand over is not asked in a hot loop", [this](auto& ls)
	{
		auto& taskSystem = System();
		const auto worker = hbe::TaskSystem::GetIOTaskStreamIndex() + 1;

		if (!taskSystem.HasStream(worker))
		{
			ls << "This engine has no worker stream past the IO stream." << lferr;

			return;
		}

		DrainObservation observation;
		DrainingProvider provider("DrainIdleProbe", taskSystem, observation);
		provider.produceResult = false;
		provider.AttachTo(worker, StreamDrainPolicy::ELane::Fifo);

		const bool askedOnce = WaitFor([&]() { return provider.GetAsks() > 0; }, 500);

		std::this_thread::sleep_for(std::chrono::milliseconds(300));
		const int asksDuringIdle = provider.GetAsks();

		provider.DetachAll();

		if (!askedOnce)
		{
			ls << "A provider on an empty lane was never asked, so the drain does not run at all." << lferr;
		}

		if (provider.GetHanded() != 0)
		{
			ls << "A provider reporting nothing produced " << provider.GetHanded() << " item(s)." << lferr;
		}

		if (asksDuringIdle > 200)
		{
			ls << "Reporting nothing led to " << asksDuringIdle
			   << " asks inside 300ms. Once per pass is the rule (R39); a hot loop turns an idle provider into a busy"
			   << " core and starves the lane it is idle on." << lferr;
		}
	});

	AddTest("Both lanes of a stream drain their own providers", [this](auto& ls)
	{
		auto& taskSystem = System();
		const auto worker = hbe::TaskSystem::GetIOTaskStreamIndex() + 1;

		if (!taskSystem.HasStream(worker))
		{
			ls << "This engine has no worker stream past the IO stream." << lferr;

			return;
		}

		DrainObservation fifoRuns;
		DrainObservation priorityRuns;
		DrainingProvider fifoProvider("LaneFifoProbe", taskSystem, fifoRuns);
		DrainingProvider priorityProvider("LanePriorityProbe", taskSystem, priorityRuns);

		fifoProvider.AttachTo(worker, StreamDrainPolicy::ELane::Fifo);
		priorityProvider.AttachTo(worker, StreamDrainPolicy::ELane::Priority);

		const bool bothServed = WaitFor([&]() {
			return fifoRuns.runs.load(std::memory_order_relaxed) > 0 &&
				   priorityRuns.runs.load(std::memory_order_relaxed) > 0;
		}, 500);

		const unsigned fifo = fifoRuns.runs.load(std::memory_order_relaxed);
		const unsigned priority = priorityRuns.runs.load(std::memory_order_relaxed);
		const int fifoStream = fifoRuns.ranOnStream.load(std::memory_order_relaxed);
		const int priorityStream = priorityRuns.ranOnStream.load(std::memory_order_relaxed);

		if (!bothServed)
		{
			ls << "A provider attached to only one lane is served " << fifo << " time(s) and the other lane "
			   << priority << " time(s), so one lane list is not being drained at all. Naming a lane is the whole of"
			   << " R35, and recording an attachment in the provider proves nothing about the stream's list." << lferr;
		}

		if (fifo > 0 && fifoStream != static_cast<int>(worker))
		{
			ls << "The FIFO-lane provider's work ran on stream " << fifoStream << " instead of " << worker << "."
			   << lferr;
		}

		if (priority > 0 && priorityStream != static_cast<int>(worker))
		{
			ls << "The priority-lane provider's work ran on stream " << priorityStream << " instead of " << worker
			   << "." << lferr;
		}

		fifoProvider.DetachAll();
		priorityProvider.DetachAll();
		fifoProvider.ReleaseItems();
		priorityProvider.ReleaseItems();
	});

	AddTest("DetachAll waits for a Produce that is already in flight", [this](auto& ls)
	{
		auto& taskSystem = System();
		const auto worker = hbe::TaskSystem::GetIOTaskStreamIndex() + 1;

		if (!taskSystem.HasStream(worker))
		{
			ls << "This engine has no worker stream past the IO stream." << lferr;

			return;
		}

		BlockingProvider provider("DetachInFlightProbe", taskSystem);
		provider.AttachTo(worker, StreamDrainPolicy::ELane::Fifo);

		const bool entered = WaitFor([&]() { return provider.inside.load(std::memory_order_acquire); }, 500);

		if (!entered)
		{
			provider.release.store(true, std::memory_order_release);
			ls << "The stream never entered Produce, so the detach had nothing to wait for and this test proved"
			   << " nothing about the window it exists to close." << lferr;

			return;
		}

		std::atomic<bool> detachReturned{false};
		std::thread detacher([&provider, &detachReturned]()
		{
			provider.DetachAll();
			detachReturned.store(true, std::memory_order_release);
		});

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		const bool returnedEarly = detachReturned.load(std::memory_order_acquire);

		provider.release.store(true, std::memory_order_release);
		detacher.join();

		if (returnedEarly)
		{
			ls << "DetachAll returned while Produce was still running, so the stream could have held a pointer to an"
			   << " object being destroyed. R40 holds the queue lock across Produce precisely so that a detach waits"
			   << " on that lock." << lferr;
		}

		if (!detachReturned.load(std::memory_order_acquire))
		{
			ls << "DetachAll never returned even after the drain was released, so waiting on the queue lock deadlocks"
			   << " rather than blocking." << lferr;
		}

		if (taskSystem.GetStream(worker).IsProviderAttached(provider, StreamDrainPolicy::ELane::Fifo))
		{
			ls << "The provider stayed registered on the stream after a completed DetachAll." << lferr;
		}
	});

	AddTest("An item whose task is gone is dropped and the lane keeps working", [this](auto& ls)
	{
		auto& taskSystem = System();
		const auto worker = hbe::TaskSystem::GetIOTaskStreamIndex() + 1;

		if (!taskSystem.HasStream(worker))
		{
			ls << "This engine has no worker stream past the IO stream." << lferr;

			return;
		}

		DrainObservation deadRuns;
		DrainingProvider deadProvider("DeadItemProbe", taskSystem, deadRuns);
		deadProvider.releaseImmediately = true;
		deadProvider.AttachTo(worker, StreamDrainPolicy::ELane::Fifo);

		const bool askedForDead = WaitFor([&]() { return deadProvider.GetAsks() >= 4; }, 500);

		deadProvider.DetachAll();

		if (!askedForDead)
		{
			ls << "The drain asked the dead-item provider only " << deadProvider.GetAsks()
			   << " time(s), so the test could not show what happens to an item naming a released task." << lferr;
		}

		if (deadRuns.runs.load(std::memory_order_relaxed) != 0)
		{
			ls << "Work ran " << deadRuns.runs.load(std::memory_order_relaxed)
			   << " time(s) from items whose tasks had already been released, so a provider can smuggle dead work past"
			   << " the liveness check (R7)." << lferr;
		}

		DrainObservation liveRuns;
		DrainingProvider liveProvider("AfterDeadItemProbe", taskSystem, liveRuns);
		liveProvider.AttachTo(worker, StreamDrainPolicy::ELane::Fifo);

		const bool laneSurvived = WaitFor([&]() { return liveRuns.runs.load(std::memory_order_relaxed) > 0; }, 500);

		liveProvider.DetachAll();
		liveProvider.ReleaseItems();

		if (!laneSurvived)
		{
			ls << "After dropping dead items the stream never ran anything again, so a released task does not merely"
			   << " lose its work - it wedges the lane." << lferr;
		}
	});

	AddTest("Both lanes of one stream share one slot and keep their own bits", [this](auto& ls)
	{
		RecordingProvider provider("ProviderLaneMaskProbe", System());
		provider.AttachTo(3, StreamDrainPolicy::ELane::Fifo);
		provider.AttachTo(3, StreamDrainPolicy::ELane::Priority);
		provider.AttachTo(3, StreamDrainPolicy::ELane::Fifo);

		if (provider.GetAttachedCount() != 1)
		{
			ls << "Attaching stream 3 on two lanes took " << provider.GetAttachedCount()
			   << " slots. One stream is one slot holding a lane mask, and two slots would be asked twice per drain."
			   << lferr;
		}

		const auto lanes = provider.GetAttachedLanes(3);
		if (lanes != (TaskProvider::LaneBitFifo | TaskProvider::LaneBitPriority))
		{
			ls << "Stream 3 holds lane mask " << static_cast<unsigned int>(lanes)
			   << ". Both lanes have to be recorded: which lane asks decides the policy applied to the work, and the"
			   << " repeat Fifo attach must not have dropped the Priority bit." << lferr;
		}

		if (provider.GetAttachedLanes(4) != 0)
		{
			ls << "A stream with no attachment reported lane mask "
			   << static_cast<unsigned int>(provider.GetAttachedLanes(4)) << "." << lferr;
		}

		provider.AttachTo(3, StreamDrainPolicy::ELane::None);
		if (provider.GetAttachedCount() != 1)
		{
			ls << "An attachment naming no lane took a slot. Nothing drains a lane-less attachment, so it would sit"
			   << " there forever and the caller would believe it was attached." << lferr;
		}

		if (provider.GetAttachedLanes(3) != (TaskProvider::LaneBitFifo | TaskProvider::LaneBitPriority))
		{
			ls << "Naming no lane still wrote to the lane mask of stream 3." << lferr;
		}
	});

	AddTest("Produce reporting nothing does not lose the attachment", [this](auto& ls)
	{
		RecordingProvider provider("ProviderIdleProbe", System());
		provider.AttachTo(0, StreamDrainPolicy::ELane::Fifo);
		provider.produceResult = false;

		const auto produced = provider.Produce(TaskProduceContext::ForStream(0));
		const bool handedSomething = produced.has_value();

		if (handedSomething)
		{
			ls << "Produce handed work over when the provider had none, so a drain would never end." << lferr;
		}

		if (!provider.IsAttachedTo(0))
		{
			ls << "Reporting no work detached the provider; an idle provider must be asked again next drain." << lferr;
		}
	});

	AddTest("AttachTo is idempotent", [this](auto& ls)
	{
		RecordingProvider provider("ProviderAttachProbe", System());
		provider.AttachTo(2, StreamDrainPolicy::ELane::Fifo);
		provider.AttachTo(2, StreamDrainPolicy::ELane::Fifo);
		provider.AttachTo(5, StreamDrainPolicy::ELane::Fifo);

		if (provider.GetAttachedCount() != 2)
		{
			ls << "Attaching stream 2 twice left " << provider.GetAttachedCount()
			   << " attachments; a duplicate makes the provider produce twice per drain." << lferr;
		}

		if (!provider.IsAttachedTo(2) || !provider.IsAttachedTo(5) || provider.IsAttachedTo(7))
		{
			ls << "Attachment membership does not match what was attached." << lferr;
		}

		if (provider.GetAttachedStream(0) != 2 || provider.GetAttachedStream(1) != 5)
		{
			ls << "Attachments are not readable in the order they were made." << lferr;
		}
	});

	AddTest("Attachment list fills to its cap", [this](auto& ls)
	{
		RecordingProvider provider("ProviderCapProbe", System());

		for (TStreamIndex stream = 0; stream < TaskProvider::MaxAttachedStreams; ++stream)
		{
			provider.AttachTo(stream, StreamDrainPolicy::ELane::Fifo);
		}

		if (provider.GetAttachedCount() != TaskProvider::MaxAttachedStreams)
		{
			ls << "Filling to the cap left " << provider.GetAttachedCount() << " of "
			   << TaskProvider::MaxAttachedStreams << " attachments." << lferr;
		}

		if (provider.IsAttachedTo(TaskProvider::MaxAttachedStreams))
		{
			ls << "A stream beyond the cap reports itself attached." << lferr;
		}
	});

	AddTest("Stop and handle reach the same state", [this](auto& ls)
	{
		RecordingProvider provider("ProviderStopProbe", System());
		provider.AttachTo(1, StreamDrainPolicy::ELane::Fifo);

		const auto handle = provider.GetHandle();
		if (!handle)
		{
			ls << "A handle taken from a live provider controls nothing." << lferr;
		}

		if (provider.IsStopRequested() || handle.IsStopRequested())
		{
			ls << "A brand new provider reports a stop." << lferr;
		}

		provider.Stop();
		if (!handle.IsStopRequested())
		{
			ls << "Provider::Stop was invisible to the handle for it." << lferr;
		}

		RecordingProvider other("ProviderStopProbe2", System());
		const auto otherHandle = other.GetHandle();
		otherHandle.RequestStop();
		if (!other.IsStopRequested())
		{
			ls << "RequestStop through a handle did not reach the provider." << lferr;
		}

		if (provider.IsStopRequested() != handle.IsStopRequested())
		{
			ls << "Provider and handle disagree about stop state." << lferr;
		}
	});

	AddTest("Stop leaves attachments for the stream to apply", [this](auto& ls)
	{
		RecordingProvider provider("ProviderDetachProbe", System());
		provider.AttachTo(4, StreamDrainPolicy::ELane::Fifo);

		provider.Stop();

		if (!provider.IsAttachedTo(4))
		{
			ls << "Stop detached the provider from another thread's list; the stream iterating it would be racing."
			   << lferr;
		}
	});

	AddTest("Empty handle is inert", [this](auto& ls)
	{
		const TaskHandle empty;

		if (empty)
		{
			ls << "A default-constructed handle claims to control a provider." << lferr;
		}

		if (empty.GetProvider() != nullptr)
		{
			ls << "A default-constructed handle carries a provider pointer." << lferr;
		}

		if (empty.IsStopRequested())
		{
			ls << "An empty handle reports a stop request." << lferr;
		}

		empty.RequestStop();
	});
}

#endif //__UNIT_TEST__
