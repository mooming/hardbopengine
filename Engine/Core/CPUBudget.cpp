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

	const auto span = OS::GetThreadCPUTime() - taskStart;
	accumulatedNanos.fetch_add(span.count(), std::memory_order_relaxed);
	isMeasuring = false;
}

std::chrono::nanoseconds hbe::CPUBudget::GetAccumulated() const noexcept
{
	return std::chrono::nanoseconds{accumulatedNanos.load(std::memory_order_relaxed)};
}

void hbe::CPUBudget::Reset() noexcept
{
	accumulatedNanos.store(0, std::memory_order_relaxed);
	isMeasuring = false;
}

void hbe::CPUBudget::RequestAllowance(std::chrono::duration<double> newAllowance) noexcept
{
	const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(newAllowance).count();

	requestedAllowanceNanos.store(nanos, std::memory_order_release);
}

std::optional<std::chrono::duration<double>> hbe::CPUBudget::ApplyRequestedAllowance() noexcept
{
	const long long requested = requestedAllowanceNanos.exchange(PendingAllowanceNone, std::memory_order_acq_rel);

	if (requested == PendingAllowanceNone)
	{
		return std::nullopt;
	}

	allowance = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::nanoseconds(requested));

	return allowance;
}

bool hbe::CPUBudget::CanTakeWork() const noexcept
{
	if (allowance.count() <= 0.0)
	{
		return true;
	}

	return GetAccumulated() < std::chrono::duration_cast<std::chrono::nanoseconds>(allowance);
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
	AddTest("A requested allowance is applied once by the owner and the latest request wins", [this](auto& ls)
	{
		using namespace std::chrono;

		CPUBudget budget;

		if (budget.ApplyRequestedAllowance().has_value())
		{
			ls << "A brand new budget had a request pending, so the first pass of every stream would apply a figure"
			   << " nobody asked for." << lferr;
		}

		budget.RequestAllowance(milliseconds(5));

		const auto applied = budget.ApplyRequestedAllowance();
		if (!applied.has_value())
		{
			ls << "A pending request was not applied, so a budget requested from another thread never takes effect"
			   << " and the stream runs on the wrong allowance forever." << lferr;
		}

		if (duration_cast<milliseconds>(applied.value_or(duration<double>{})).count() != 5)
		{
			ls << "The applied allowance is not the 5ms that was requested, so the request is being stored in a unit"
			   << " that does not survive the round trip." << lferr;
		}

		if (duration_cast<milliseconds>(budget.GetAllowance()).count() != 5)
		{
			ls << "GetAllowance does not show the applied request, so the unsynchronised field this budget reads every"
			   << " pass was never written - the request path would be a no-op that looks correct." << lferr;
		}

		if (budget.ApplyRequestedAllowance().has_value())
		{
			ls << "The same request applied twice, so the pending marker is not cleared and the stream re-applies an"
			   << " old figure every pass." << lferr;
		}

		budget.RequestAllowance(milliseconds(2));
		budget.RequestAllowance(milliseconds(1));

		const auto latest = budget.ApplyRequestedAllowance();
		if (duration_cast<milliseconds>(latest.value_or(duration<double>{})).count() != 1)
		{
			ls << "Two requests before a single pass did not resolve to the latest one. An allowance is a state, not a"
			   << " queue of events, so asking for 2ms then 1ms must not bill the stream for both." << lferr;
		}
	});

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
