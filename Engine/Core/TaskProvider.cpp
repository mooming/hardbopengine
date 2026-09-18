// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TaskProvider.h"

#include "Core/Debug.h"

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

TaskProvider::TaskProvider(StaticString providerName) noexcept
	: name(providerName)
{
}

void TaskProvider::AttachTo(TStreamIndex stream) noexcept
{
	for (TStreamIndex index = 0; index < attachedCount; ++index)
	{
		if (attached[static_cast<size_t>(index)] == stream)
		{
			return;
		}
	}

	if (attachedCount >= MaxAttachedStreams)
	{
		Assert(false, "TaskProvider ", name, " is already attached to its maximum of ", MaxAttachedStreams,
			   " streams; this attachment was dropped");
		return;
	}

	attached[static_cast<size_t>(attachedCount)] = stream;
	++attachedCount;
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

/// @brief Provider that records what it was handed, so the interface can be observed rather than assumed.
class RecordingProvider final : public hbe::TaskProvider
{
public:
	using TaskProvider::TaskProvider;

	int produceCalls = 0;
	hbe::TStreamIndex lastStream = -1;
	hbe::time::TEngineTimePoint lastNow{};
	bool produceResult = true;

	bool Produce(const hbe::TaskProduceContext& context) noexcept override
	{
		++produceCalls;
		lastStream = context.stream;
		lastNow = context.now;
		return produceResult;
	}
};

} // namespace

void hbe::TaskProviderTest::Prepare()
{
	AddTest("Produce is handed the stream and a live clock reading", [this](auto& ls)
	{
		RecordingProvider provider("ProviderContextProbe");

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

	AddTest("Produce reporting nothing does not lose the attachment", [this](auto& ls)
	{
		RecordingProvider provider("ProviderIdleProbe");
		provider.AttachTo(0);
		provider.produceResult = false;

		const bool produced = provider.Produce(TaskProduceContext::ForStream(0));

		if (produced)
		{
			ls << "Produce reported work when the provider had none, so a drain would never end." << lferr;
		}

		if (!provider.IsAttachedTo(0))
		{
			ls << "Reporting no work detached the provider; an idle provider must be asked again next drain." << lferr;
		}
	});

	AddTest("AttachTo is idempotent", [this](auto& ls)
	{
		RecordingProvider provider("ProviderAttachProbe");
		provider.AttachTo(2);
		provider.AttachTo(2);
		provider.AttachTo(5);

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
		RecordingProvider provider("ProviderCapProbe");

		for (TStreamIndex stream = 0; stream < TaskProvider::MaxAttachedStreams; ++stream)
		{
			provider.AttachTo(stream);
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
		RecordingProvider provider("ProviderStopProbe");
		provider.AttachTo(1);

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

		RecordingProvider other("ProviderStopProbe2");
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
		RecordingProvider provider("ProviderDetachProbe");
		provider.AttachTo(4);

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
