// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <chrono>
#include <optional>

namespace hbe
{

/// @brief One stream's allowance of measured CPU time, configured as a duration and spent as work runs.
/// @details A stream asks CanTakeWork before it dequeues, and stops taking work while the allowance is
///          spent. Nothing is preempted: a task already running runs to completion, so a single long task
///          overshoots the allowance by its own length. That is the intended shape - the overshoot is
///          bounded by the longest task, not by the budget.
/// @details The allowance is configured in seconds because that is the unit a caller can reason about and
///          put in a config file, while spending is measured in the thread's own CPU time. The two are
///          different quantities on purpose: a stream may legitimately spend less CPU than the wall time
///          its tasks took, and a budget that charged wall time would bill a stream for work that was only
///          ever blocked.
/// @details An allowance of zero means unlimited, which is what a stream that has not been given a budget
///          gets, so the primitive is inert until something configures it.
/// @note Not thread-safe for the things that matter, and does not pretend to be. A budget belongs to
///       exactly one stream thread, which is the thread whose CPU time is charged; sharing the pairing
///       across threads would measure the span between two unrelated threads and call it a task.
/// @note The accumulated figure is a separate case and is atomic, because it is read for diagnosis from
///       threads that do not own the budget. That makes the read well-defined; it does not make BeginTask
///       and EndTask portable to another thread, and the total is only ever written by the owner.
class CPUBudget
{
private:
	static constexpr long long PendingAllowanceNone = -1;

	std::chrono::duration<double> allowance{};

	/// @brief A pending allowance in nanoseconds, or `PendingAllowanceNone` when nothing is waiting.
	/// @details Held as an integer because `duration<double>` has no atomic form worth storing, and a budget decision
	///          never needs more nanosecond resolution than this.
	std::atomic<long long> requestedAllowanceNanos{PendingAllowanceNone};

	std::atomic<long long> accumulatedNanos{0};

	std::chrono::nanoseconds taskStart{};

	bool isMeasuring = false;

public:
	/// @brief Set the allowance, expressed as a duration of CPU time. Zero means unlimited.
	/// @note Only the owning stream thread may call this, because `allowance` is deliberately unsynchronised -
	///       `CanTakeWork` reads it once per pass on that thread. Anyone else wants `RequestAllowance`.
	void Configure(std::chrono::duration<double> allowance) noexcept;

	/// @brief Ask the owning stream to change its allowance, from whichever thread the caller is on.
	/// @details `allowance` is a plain field on purpose, so writing it from elsewhere is a data race rather than a late
	///          write. A request stores one value atomically and the owner applies it, which is the same hand-off the
	///          accounting window already uses: the pass signals and the stream applies to itself.
	/// @note The most recent request wins. An unapplied earlier request is not queued and is not observable - the
	///       allowance is a state, not an event, and a stream that asked for 5ms and then for 1ms before its next pass
	///       should end up at 1ms, not bill itself for both.
	void RequestAllowance(std::chrono::duration<double> allowance) noexcept;

	/// @brief Apply a pending request, if there is one. Call only on the thread that owns this budget.
	/// @return The allowance the request set, or `std::nullopt` when nothing was pending.
	std::optional<std::chrono::duration<double>> ApplyRequestedAllowance() noexcept;

	/// @brief The configured allowance. Zero means unlimited.
	[[nodiscard]] std::chrono::duration<double> GetAllowance() const noexcept;

	/// @brief Start charging the calling thread's CPU time to this budget.
	/// @note Must be followed by EndTask on the same thread. Pairing them on different threads measures
	///       the span between two unrelated threads and produces a number with no meaning.
	void BeginTask() noexcept;

	/// @brief Stop charging and add the measured span to the accumulation.
	/// @note Does nothing if BeginTask has not been called since the last EndTask or Reset. Measuring from
	///       a start that was never taken would charge the thread's entire life to the budget and leave a
	///       stream refusing work forever, so the pairing error loses the charge instead of poisoning the
	///       budget - the same fail-open choice as a failed platform call in OS::GetThreadCPUTime.
	void EndTask() noexcept;

	/// @brief CPU time charged to this budget since it was last reset.
	[[nodiscard]] std::chrono::nanoseconds GetAccumulated() const noexcept;

	/// @brief Forget everything charged so far. A stream does this when its accounting window moves on.
	void Reset() noexcept;

	/// @brief Whether this budget still permits taking another task.
	/// @return True while the allowance is not spent, and always true for an unlimited budget.
	[[nodiscard]] bool CanTakeWork() const noexcept;
};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for the measured-CPU budget.
class CPUBudgetTest final : public TestCollection
{
public:
	CPUBudgetTest()
		: TestCollection("CPUBudgetTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
