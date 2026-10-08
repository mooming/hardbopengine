// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <cstdint>


namespace hbe
{
/// API reference: docs/Core/StreamDrainPolicy/index.html
class StreamDrainPolicy final
{
public:
	enum class ELane : uint8_t
	{
		None,
		Fifo,
		Priority
	};

private:
	uint32_t fifoWeight = 1;
	uint32_t priorityWeight = 1;

	std::chrono::duration<double> allowance{};

	std::chrono::nanoseconds fifoUsed{};
	std::chrono::nanoseconds priorityUsed{};

	int64_t fifoCredit = 1;
	int64_t priorityCredit = 1;

	std::chrono::nanoseconds maxAge{};

public:
	void ConfigureRate(uint32_t fifoWeight, uint32_t priorityWeight) noexcept;
	void ConfigureAllowance(std::chrono::duration<double> streamAllowance) noexcept;

	[[nodiscard]] ELane ChooseLane(bool fifoHasWork, bool priorityHasWork) const noexcept;
	void CommitTake(ELane lane) noexcept;

	void ChargeFifo(std::chrono::nanoseconds spent) noexcept;
	void ChargePriority(std::chrono::nanoseconds spent) noexcept;

	[[nodiscard]] bool IsRoundExhausted() const noexcept;
	void EndRound() noexcept;

	[[nodiscard]] std::chrono::nanoseconds GetFifoUsed() const noexcept;
	[[nodiscard]] std::chrono::nanoseconds GetPriorityUsed() const noexcept;

	[[nodiscard]] std::chrono::duration<double> GetFifoShare() const noexcept;
	[[nodiscard]] std::chrono::duration<double> GetPriorityShare() const noexcept;

	void SetMaxAge(std::chrono::nanoseconds maxAge) noexcept;
	[[nodiscard]] std::chrono::nanoseconds GetMaxAge() const noexcept;

	[[nodiscard]] uint32_t GetFifoWeight() const noexcept;
	[[nodiscard]] uint32_t GetPriorityWeight() const noexcept;

	[[nodiscard]] bool IsOverAge(std::chrono::nanoseconds offerTime, std::chrono::nanoseconds now) const noexcept;

private:
	[[nodiscard]] int64_t FifoCredit() const noexcept;
	[[nodiscard]] int64_t PriorityCredit() const noexcept;
};
} // namespace hbe

#ifdef __TEST__
#include "Test/TestCollection.h"

namespace hbe
{
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
#endif //__TEST__
