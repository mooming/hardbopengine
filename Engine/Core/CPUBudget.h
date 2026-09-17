// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>

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
/// @note Not thread-safe, and deliberately not made so. A budget belongs to exactly one stream thread,
///       which is also the thread whose CPU time is charged; sharing one across threads would measure the
///       wrong thread rather than a shared total.
class CPUBudget
{
public:
	/// @brief Set the allowance, expressed as a duration of CPU time. Zero means unlimited.
	void Configure(std::chrono::duration<double> allowance) noexcept;
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

private:
	std::chrono::duration<double> allowance{};
	std::chrono::nanoseconds accumulated{};
	std::chrono::nanoseconds taskStart{};
	bool isMeasuring = false;
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
