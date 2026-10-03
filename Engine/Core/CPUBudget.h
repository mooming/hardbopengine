// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <chrono>
#include <optional>


namespace hbe
{
/// API reference: docs/Core/CPUBudget/index.html
class CPUBudget
{
private:
	static constexpr long long PendingAllowanceNone = -1;

	std::chrono::duration<double> allowance{};

	std::atomic<long long> requestedAllowanceNanos{PendingAllowanceNone};

	std::atomic<long long> accumulatedNanos{0};

	std::chrono::nanoseconds taskStart{};

	bool isMeasuring = false;

public:
	void Configure(std::chrono::duration<double> allowance) noexcept;

	void RequestAllowance(std::chrono::duration<double> allowance) noexcept;

	std::optional<std::chrono::duration<double>> ApplyRequestedAllowance() noexcept;

	[[nodiscard]] std::chrono::duration<double> GetAllowance() const noexcept;

	void BeginTask() noexcept;

	void EndTask() noexcept;

	[[nodiscard]] std::chrono::nanoseconds GetAccumulated() const noexcept;

	void Reset() noexcept;

	[[nodiscard]] bool CanTakeWork() const noexcept;
};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
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
