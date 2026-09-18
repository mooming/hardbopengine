// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <cstdint>

namespace hbe
{

/// @brief Decides which of a stream's two task lanes is taken from next, and when the stream has to stop.
/// @details A stream holds a FIFO lane and a priority lane, and the caller states a FIFO:priority rate per
///          stream. That rate is a share of the stream's CPU allowance rather than a count of tasks: each
///          lane gets allowance x (its weight / total weight) and accumulates the CPU time of the tasks it
///          ran. Borrowing is free — a lane may keep taking while the stream total is inside budget and
///          either lane's share still has room — so an idle lane does not hold its share hostage. The
///          accepted cost of free borrowing is that the long-run ratio is not preserved when both lanes are
///          permanently backlogged: whoever has work gets the CPU.
/// @details With no allowance configured there is no CPU to divide, so the rate falls back to a weighted
///          rotation over takes. Both modes honour the same promise: a lane with work is never starved by
///          the other lane's weight.
/// @note A task cannot be stopped once taken, so a lane overshoots its share by the length of its longest
///       task. The decision is a gate on ACQUIRING work, never a ceiling on execution, and no caller may
///       read "share spent" as "nothing is in flight".
/// @note Not thread-safe by design: the owning stream thread makes every decision and takes every charge.
///       Reads of counters for reporting are exposed as separate const accessors and are only meaningful
///       from the owning thread.
class StreamDrainPolicy final
{
public:
	/// @brief Which lane to take next, or None when the stream must not take anything.
	enum class ELane : uint8_t
	{
		None,
		Fifo,
		Priority
	};

	/// @brief Set the FIFO:priority rate.
	/// @note A weight of zero on either side is treated as one, with both falling back to 1:1 if the caller
	///       passes two zeros. A zero weight is not "never serve this lane" — a lane that can never be
	///       served should not exist, and silently configuring one would strand its queued tasks.
	void ConfigureRate(uint32_t fifoWeight, uint32_t priorityWeight) noexcept;

	/// @brief Set the stream's CPU allowance, from which lane shares are derived. Zero means unlimited.
	void ConfigureAllowance(std::chrono::duration<double> streamAllowance) noexcept;

	/// @brief Choose the lane to acquire the next task from.
	/// @details Pure: it neither charges the budget nor mutates rotation state, so it may be called twice
	///          with the same arguments and give the same answer. Charge the chosen lane once its task has
	///          actually run.
	[[nodiscard]] ELane ChooseLane(bool fifoHasWork, bool priorityHasWork) const noexcept;

	/// @brief Record the CPU time a task on a lane consumed, and decrement that lane's rotation credit.
	void ChargeFifo(std::chrono::nanoseconds spent) noexcept;
	/// @brief Record the CPU time a task on the priority lane consumed.
	void ChargePriority(std::chrono::nanoseconds spent) noexcept;

	/// @brief Whether the stream total has passed its allowance, which is what ends a round.
	/// @return Always false for an unlimited stream: a round is defined by the allowance, so with none there
	///         is nothing to exhaust and a caller would never reset.
	[[nodiscard]] bool IsRoundExhausted() const noexcept;

	/// @brief Open a new round: both lane accumulations start again at zero.
	void EndRound() noexcept;

	/// @brief CPU time charged to the FIFO lane in the current round.
	[[nodiscard]] std::chrono::nanoseconds GetFifoUsed() const noexcept;
	/// @brief CPU time charged to the priority lane in the current round.
	[[nodiscard]] std::chrono::nanoseconds GetPriorityUsed() const noexcept;

	/// @brief The FIFO lane's share of the allowance, derived from the rate. Zero when unlimited.
	[[nodiscard]] std::chrono::duration<double> GetFifoShare() const noexcept;
	/// @brief The priority lane's share of the allowance, derived from the rate. Zero when unlimited.
	[[nodiscard]] std::chrono::duration<double> GetPriorityShare() const noexcept;

private:
	/// @brief Rotation credit for the unlimited case, where shares cannot be expressed in CPU time.
	/// @details Held as signed values and replenished by the weights when both sides have run out, which is
	///          what keeps a lane alive when its configured weight is smaller than the other's.
	[[nodiscard]] int64_t FifoCredit() const noexcept;
	[[nodiscard]] int64_t PriorityCredit() const noexcept;

	uint32_t fifoWeight = 1;
	uint32_t priorityWeight = 1;
	std::chrono::duration<double> allowance{};
	std::chrono::nanoseconds fifoUsed{};
	std::chrono::nanoseconds priorityUsed{};
	int64_t fifoCredit = 1;
	int64_t priorityCredit = 1;
};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for the two-lane drain decision.
class StreamDrainPolicyTest final : public TestCollection
{
public:
	StreamDrainPolicyTest()
		: TestCollection("StreamDrainPolicyTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
