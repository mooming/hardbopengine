// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

#include "Core/StreamDrainPolicy.h"
#include "Core/TaskStreamIndex.h"
#include "Core/Time.h"
#include "String/StaticString.h"

namespace hbe
{

class TaskProvider;

/// @brief Control token for one TaskProvider, held by whoever needs to stop it.
/// @details A stream cannot stop a task that has already been taken, so the thing worth holding on to is
///          the producer: stopping a provider ends the supply of work without disturbing work already in
///          flight. The token is what lets a stream, a registry, or a subsystem manager say "that one"
///          without owning the provider.
/// @note Does not own the provider and cannot keep it alive. A handle outliving its provider dangles, and
///       no check here can detect that; the owner's lifetime rule is that handles die first.
/// @threadsafe RequestStop and IsStopRequested are safe from any thread. GetProvider hands back a pointer
///             whose remaining use is limited to that same claim - reading anything on the provider other
///             than its stop state from another thread is not covered by this.
class TaskHandle final
{
public:
	TaskHandle() = default;
	explicit TaskHandle(TaskProvider& provider) noexcept;

	/// @brief True when this token refers to a provider. A default-constructed token refers to nothing.
	[[nodiscard]] explicit operator bool() const noexcept
	{
		return provider != nullptr;
	}

	/// @brief The provider this token controls, or null when it controls nothing.
	[[nodiscard]] TaskProvider* GetProvider() const noexcept
	{
		return provider;
	}

	/// @brief Whether a stop has been requested on the controlled provider.
	[[nodiscard]] bool IsStopRequested() const noexcept;

	/// @brief Ask the controlled provider to stop producing.
	/// @note Const because stopping does not alter the token: any copy of it can issue the same request,
	///       and a token that became unusable once used would have to be kept mutable everywhere.
	/// @note Doing nothing when the token is empty is deliberate, so that a registry entry that was never
	///       filled can be stopped on a teardown path without a caller-side guard.
	void RequestStop() const noexcept;

private:
	TaskProvider* provider = nullptr;
};

/// @brief Everything a stream hands its providers on a Produce call, which is deliberately two facts.
/// @details The stream is passed because a provider attached to several streams must know which one is
///          asking - it is the only way a provider can aim a successor task back at the right place. The
///          clock reading is passed rather than pulled so that every provider in one drain sees the same
///          instant, and no provider reaches for a global to find out what time it is.
/// @details Carries no remaining budget, by decision. The stream alone decides whether to call Produce at
///          all; a provider also gating on the same figure creates two authorities over one quantity, and
///          when they disagree exactly one of them is right while both look authoritative.
/// @details now is an instant in the engine epoch's own time base, not an elapsed span, so that comparing
///          two of them is valid arithmetic rather than a subtraction of two independently anchored
///          numbers. Call time::ElapsedSinceEngineEpoch to turn it into a duration.
struct TaskProduceContext final
{
	/// @brief Build the context for one drain of one stream.
	/// @details The clock reading is taken here rather than at each call site so that a single drain - and
	///          every provider inside it - shares one instant. If each provider read the clock for itself,
	///          two providers in the same drain would reason about two different times and a caller could
	///          not tell a scheduling effect from a clock effect.
	[[nodiscard]] static TaskProduceContext ForStream(TStreamIndex streamIndex) noexcept;

	TStreamIndex stream = 0;
	time::TEngineTimePoint now{};
};

/// @brief A producer of tasks, and the unit of lifecycle in the task system.
/// @details Apps, subsystems and tools are providers. A provider is asked for work rather than pushing it:
///          a stream drains a provider while the stream's budget allows and Produce keeps reporting that
///          it produced something. That makes the drain - not the individual task - the unit of control,
///          which is the only unit available given that a task cannot be stopped once taken.
/// @details Produce returning false ends the drain for this pass and costs one call per drain thereafter.
///          A separate HasWork predicate was rejected: by the time Produce runs, a HasWork answer is
///          already advisory because provider state can change in between, so the stream would be steering
///          on a reading it cannot trust.
class TaskProvider
{
public:
	/// @brief How many streams one provider may feed.
	/// @details Attachments are a lifecycle event, and the realistic figures are one or two, so the set is
	///          held inline rather than allocated: a provider needs no allocator to exist, and a provider
	///          that cannot allocate still gets to be constructed. What that reasoning justifies is a fixed
	///          inline set, not a small one: the bound has to clear the number of streams an ordinary machine
	///          has, because feeding every worker stream is a thing providers do (measured: twelve hardware
	///          threads here give ten worker streams, and R35's lanes make both lanes on every worker twenty
	///          attachments). Exceeding this asserts, reports the provider, the stream and the capacity, and
	///          drops the attachment - never a silent drop, because the symptom of a lost attachment is a
	///          stream that quietly never gets work.
	static constexpr TStreamIndex MaxAttachedStreams = 64;

	/// @brief The lane bits an attachment may name, one per lane of a stream.
	/// @details Named explicitly rather than derived from `ELane`'s values: `ELane::None` occupies zero, so a
	///          positional encoding would give "no lane" a bit of its own and a stream could match it.
	static constexpr std::uint8_t LaneBitFifo = 1U << 0;
	static constexpr std::uint8_t LaneBitPriority = 1U << 1;

	/// @brief The lane mask an `ELane` claims, or zero for `ELane::None`.
	[[nodiscard]] static constexpr std::uint8_t LaneBit(StreamDrainPolicy::ELane lane) noexcept
	{
		return lane == StreamDrainPolicy::ELane::Fifo
					   ? LaneBitFifo
					   : (lane == StreamDrainPolicy::ELane::Priority ? LaneBitPriority : 0U);
	}

	explicit TaskProvider(StaticString name) noexcept;
	virtual ~TaskProvider() = default;

	TaskProvider(const TaskProvider&) = delete;
	TaskProvider& operator=(const TaskProvider&) = delete;

	/// @brief Produce work into the stream named by the context, and report whether anything was produced.
	/// @details Called on that stream's schedule, possibly several times in a row while the stream's budget
	///          lasts. Returning false says "nothing right now" and ends this drain; it is not an error and
	///          does not detach the provider.
	/// @note Implementations must be cheap enough to call when idle, since an idle provider is asked once
	///       per drain and that call is the price of not keeping a second source of truth about intent.
	virtual bool Produce(const TaskProduceContext& context) noexcept = 0;

	/// @brief Start being drained by one lane of one stream. Naming the lane is not decoration: which lane
	///        attaches decides the policy the stream applies to what this provider hands it (R35).
	/// @note Duplicates are per (stream, lane). Attaching the same lane twice changes nothing, because a stream
	///       that listed a provider twice on one lane would call Produce twice per drain and double its output
	///       rate silently. Attaching the other lane is a different thing and is allowed: it says this provider
	///       may be drawn on by either lane, which is the same doubled-call hazard taken with intent, so it has
	///       to be the caller's decision and never a default.
	/// @note `ELane::None` names no lane and attaches nothing; it is reported rather than accepted, because an
	///       attachment with no lane would sit in the slot and never be drained.
	void AttachTo(TStreamIndex stream, StreamDrainPolicy::ELane lane) noexcept;

	/// @brief How many streams this provider currently feeds.
	[[nodiscard]] TStreamIndex GetAttachedCount() const noexcept
	{
		return attachedCount;
	}

	/// @brief The attached stream held at a position in attachment order. Out-of-range asserts and returns 0.
	[[nodiscard]] TStreamIndex GetAttachedStream(TStreamIndex index) const noexcept;
	/// @brief Whether a given stream is currently draining this provider on any lane.
	[[nodiscard]] bool IsAttachedTo(TStreamIndex stream) const noexcept;

	/// @brief The lane mask this provider is attached to `stream` on, or zero if it is not attached to it.
	[[nodiscard]] std::uint8_t GetAttachedLanes(TStreamIndex stream) const noexcept;

	/// @brief Request that this provider stop producing.
	/// @details Sets a flag; it does not remove the provider from any stream's list, because the streams
	///          reading that list run on their own threads and a caller here would be editing a container
	///          while someone else iterates it. Each stream applies the request when it next looks, which
	///          keeps list mutation on the thread that owns the list.
	/// @details Stop is not cancellation. Tasks already taken run to completion - the task system cannot
	///          stop one - so "stopped" means no further Produce calls, not no further work.
	void Stop() noexcept;

	/// @brief Whether a stop has been requested, by this provider or through any handle to it.
	/// @threadsafe Readable from any thread.
	[[nodiscard]] bool IsStopRequested() const noexcept
	{
		return stopRequested.load(std::memory_order_acquire);
	}

	/// @brief A token that can request this provider's stop from elsewhere.
	[[nodiscard]] TaskHandle GetHandle() noexcept;

	[[nodiscard]] StaticString GetName() const noexcept
	{
		return name;
	}

private:
	StaticString name;
	std::array<TStreamIndex, static_cast<size_t>(MaxAttachedStreams)> attached{};
	std::array<std::uint8_t, static_cast<size_t>(MaxAttachedStreams)> lanes{};
	TStreamIndex attachedCount = 0;
	std::atomic<bool> stopRequested{false};
};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for the provider interface and its control token.
class TaskProviderTest final : public TestCollection
{
public:
	TaskProviderTest()
		: TestCollection("TaskProviderTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
