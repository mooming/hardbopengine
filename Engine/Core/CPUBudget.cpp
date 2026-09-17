// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "CPUBudget.h"

#include <thread>

#include "OSAL/OSThread.h"

void hbe::CPUBudget::Configure(std::chrono::duration<double> newAllowance) noexcept
{
	allowance = newAllowance;
}

std::chrono::duration<double> hbe::CPUBudget::GetAllowance() const noexcept
{
	return allowance;
}

void hbe::CPUBudget::BeginTask() noexcept
{
	taskStart = OS::GetThreadCPUTime();
	isMeasuring = true;
}

void hbe::CPUBudget::EndTask() noexcept
{
	if (!isMeasuring)
	{
		return;
	}

	accumulated += OS::GetThreadCPUTime() - taskStart;
	isMeasuring = false;
}

std::chrono::nanoseconds hbe::CPUBudget::GetAccumulated() const noexcept
{
	return accumulated;
}

void hbe::CPUBudget::Reset() noexcept
{
	accumulated = std::chrono::nanoseconds::zero();
	isMeasuring = false;
}

bool hbe::CPUBudget::CanTakeWork() const noexcept
{
	if (allowance.count() <= 0.0)
	{
		return true;
	}

	return accumulated < std::chrono::duration_cast<std::chrono::nanoseconds>(allowance);
}

#ifdef __UNIT_TEST__
namespace
{

/// @brief Keep the calling thread busy until it has consumed the given CPU time, or gives up.
/// @details Bounded by CPU time rather than an iteration count so the same test means the same thing on a
///          slow core, under a debugger, and in Release where an iteration count would be meaningless.
void BurnCPU(std::chrono::milliseconds target) noexcept
{
	const auto start = OS::GetThreadCPUTime();
	unsigned long long sink = 0;

	while (sink < 400000000ULL && OS::GetThreadCPUTime() - start < target)
	{
		++sink;
	}
}

} // namespace

void hbe::CPUBudgetTest::Prepare()
{
	AddTest("Unlimited budget always accepts work", [this](auto& ls)
	{
		CPUBudget budget;
		budget.Configure(std::chrono::duration<double>{});

		budget.BeginTask();
		BurnCPU(std::chrono::milliseconds(10));
		budget.EndTask();

		if (budget.GetAccumulated() <= std::chrono::nanoseconds::zero())
		{
			ls << "Busy work charged nothing to the budget, so this test measured nothing." << lferr;
		}

		if (!budget.CanTakeWork())
		{
			ls << "An unlimited budget refused work." << lferr;
		}
	});

	AddTest("Measured work spends the allowance", [this](auto& ls)
	{
		CPUBudget budget;
		budget.Configure(std::chrono::duration<double, std::milli>{2});

		budget.BeginTask();
		BurnCPU(std::chrono::milliseconds(12));
		budget.EndTask();

		if (budget.CanTakeWork())
		{
			ls << "A budget still accepted work after charging "
			   << std::chrono::duration_cast<std::chrono::microseconds>(budget.GetAccumulated()).count()
			   << " us against a 2 ms allowance." << lferr;
		}
	});

	AddTest("Waiting does not spend the allowance", [this](auto& ls)
	{
		CPUBudget budget;
		budget.Configure(std::chrono::duration<double, std::milli>{10});

		budget.BeginTask();
		std::this_thread::sleep_for(std::chrono::milliseconds(80));
		budget.EndTask();

		const auto charged = std::chrono::duration_cast<std::chrono::milliseconds>(budget.GetAccumulated()).count();
		if (charged > 5)
		{
			ls << "An 80 ms sleep charged " << charged << " ms of CPU to the budget; waiting is being billed as work."
			   << lferr;
		}

		if (!budget.CanTakeWork())
		{
			ls << "A budget that only ever waited refused work, with "
			   << std::chrono::duration_cast<std::chrono::microseconds>(budget.GetAccumulated()).count()
			   << " us charged." << lferr;
		}
	});

	AddTest("Reset restores the allowance", [this](auto& ls)
	{
		CPUBudget budget;
		budget.Configure(std::chrono::duration<double, std::milli>{2});

		budget.BeginTask();
		BurnCPU(std::chrono::milliseconds(12));
		budget.EndTask();

		if (budget.CanTakeWork())
		{
			ls << "Expected the allowance to be spent before testing Reset." << lferr;
		}

		budget.Reset();

		if (budget.GetAccumulated() != std::chrono::nanoseconds::zero())
		{
			ls << "Reset left "
			   << std::chrono::duration_cast<std::chrono::microseconds>(budget.GetAccumulated()).count()
			   << " us accumulated." << lferr;
		}

		if (!budget.CanTakeWork())
		{
			ls << "A reset budget still refused work." << lferr;
		}
	});

	AddTest("Unmatched EndTask charges nothing", [this](auto& ls)
	{
		CPUBudget budget;
		budget.BeginTask();
		budget.EndTask();

		const auto afterPair = budget.GetAccumulated();
		budget.EndTask();

		if (budget.GetAccumulated() != afterPair)
		{
			ls << "An EndTask without a matching BeginTask charged "
			   << std::chrono::duration_cast<std::chrono::seconds>(budget.GetAccumulated() - afterPair).count()
			   << " s - the thread's whole life, not a task." << lferr;
		}
	});
}

#endif //__UNIT_TEST__
