// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "StreamDrainPolicy.h"


namespace hbe
{
void StreamDrainPolicy::ConfigureRate(uint32_t newFifoWeight, uint32_t newPriorityWeight) noexcept
{
	fifoWeight = newFifoWeight > 0 ? newFifoWeight : 1;
	priorityWeight = newPriorityWeight > 0 ? newPriorityWeight : 1;

	fifoCredit = static_cast<int64_t>(fifoWeight);
	priorityCredit = static_cast<int64_t>(priorityWeight);
}

void StreamDrainPolicy::ConfigureAllowance(std::chrono::duration<double> newAllowance) noexcept
{
	allowance = newAllowance.count() > 0.0 ? newAllowance : std::chrono::duration<double>{};
}

StreamDrainPolicy::ELane StreamDrainPolicy::ChooseLane(bool fifoHasWork, bool priorityHasWork) const noexcept
{
	if (!fifoHasWork && !priorityHasWork)
	{
		return ELane::None;
	}

	const bool unlimited = allowance.count() <= 0.0;
	if (unlimited)
	{
		if (fifoHasWork && priorityHasWork)
		{
			return fifoCredit >= priorityCredit ? ELane::Fifo : ELane::Priority;
		}

		return fifoHasWork ? ELane::Fifo : ELane::Priority;
	}

	const auto used = fifoUsed + priorityUsed;
	if (used >= std::chrono::duration_cast<std::chrono::nanoseconds>(allowance))
	{
		return ELane::None;
	}

	const bool fifoHasRoom =
			fifoHasWork && fifoUsed < std::chrono::duration_cast<std::chrono::nanoseconds>(GetFifoShare());
	const bool priorityHasRoom =
			priorityHasWork && priorityUsed < std::chrono::duration_cast<std::chrono::nanoseconds>(GetPriorityShare());

	if (fifoHasRoom && priorityHasRoom)
	{
		const auto fifoRemaining = std::chrono::duration_cast<std::chrono::nanoseconds>(GetFifoShare()) - fifoUsed;
		const auto priorityRemaining =
				std::chrono::duration_cast<std::chrono::nanoseconds>(GetPriorityShare()) - priorityUsed;

		return priorityRemaining > fifoRemaining ? ELane::Priority : ELane::Fifo;
	}

	if (fifoHasRoom)
	{
		return ELane::Fifo;
	}

	if (priorityHasRoom)
	{
		return ELane::Priority;
	}

	if (fifoHasWork && priorityHasWork)
	{
		return priorityWeight > fifoWeight ? ELane::Priority : ELane::Fifo;
	}

	return fifoHasWork ? ELane::Fifo : ELane::Priority;
}

void StreamDrainPolicy::ChargeFifo(std::chrono::nanoseconds spent) noexcept
{
	fifoUsed += spent;
}

void StreamDrainPolicy::ChargePriority(std::chrono::nanoseconds spent) noexcept
{
	priorityUsed += spent;
}

void StreamDrainPolicy::CommitTake(ELane lane) noexcept
{
	if (lane == ELane::Fifo)
	{
		--fifoCredit;
	}
	else if (lane == ELane::Priority)
	{
		--priorityCredit;
	}

	if (fifoCredit <= 0 && priorityCredit <= 0)
	{
		fifoCredit += static_cast<int64_t>(fifoWeight);
		priorityCredit += static_cast<int64_t>(priorityWeight);
	}
}

bool StreamDrainPolicy::IsRoundExhausted() const noexcept
{
	if (allowance.count() <= 0.0)
	{
		return false;
	}

	return (fifoUsed + priorityUsed) >= std::chrono::duration_cast<std::chrono::nanoseconds>(allowance);
}

void StreamDrainPolicy::EndRound() noexcept
{
	fifoUsed = std::chrono::nanoseconds::zero();
	priorityUsed = std::chrono::nanoseconds::zero();
	fifoCredit = static_cast<int64_t>(fifoWeight);
	priorityCredit = static_cast<int64_t>(priorityWeight);
}

std::chrono::nanoseconds StreamDrainPolicy::GetFifoUsed() const noexcept
{
	return fifoUsed;
}

std::chrono::nanoseconds StreamDrainPolicy::GetPriorityUsed() const noexcept
{
	return priorityUsed;
}

void StreamDrainPolicy::SetMaxAge(std::chrono::nanoseconds newMaxAge) noexcept
{
	maxAge = newMaxAge;
}

std::chrono::nanoseconds StreamDrainPolicy::GetMaxAge() const noexcept
{
	return maxAge;
}

bool StreamDrainPolicy::IsOverAge(std::chrono::nanoseconds offerTime, std::chrono::nanoseconds now) const noexcept
{
	return maxAge.count() > 0 && offerTime.count() > 0 && now - offerTime > maxAge;
}

uint32_t StreamDrainPolicy::GetFifoWeight() const noexcept
{
	return fifoWeight;
}

uint32_t StreamDrainPolicy::GetPriorityWeight() const noexcept
{
	return priorityWeight;
}

std::chrono::duration<double> StreamDrainPolicy::GetFifoShare() const noexcept
{
	if (allowance.count() <= 0.0)
	{
		return std::chrono::duration<double>{};
	}

	return allowance * (static_cast<double>(fifoWeight) / static_cast<double>(fifoWeight + priorityWeight));
}

std::chrono::duration<double> StreamDrainPolicy::GetPriorityShare() const noexcept
{
	if (allowance.count() <= 0.0)
	{
		return std::chrono::duration<double>{};
	}

	return allowance * (static_cast<double>(priorityWeight) / static_cast<double>(fifoWeight + priorityWeight));
}

int64_t StreamDrainPolicy::FifoCredit() const noexcept
{
	return fifoCredit;
}

int64_t StreamDrainPolicy::PriorityCredit() const noexcept
{
	return priorityCredit;
}
} // namespace hbe

#ifdef __TEST__
namespace
{
using Lane = hbe::StreamDrainPolicy::ELane;

struct LaneCounts
{
	int fifo = 0;
	int priority = 0;
	int none = 0;
};

LaneCounts DriveTakes(hbe::StreamDrainPolicy& policy, int takes)
{
	LaneCounts counts;

	for (int take = 0; take < takes; ++take)
	{
		const auto lane = policy.ChooseLane(true, true);
		switch (lane)
		{
			case Lane::Fifo:
				++counts.fifo;
				policy.CommitTake(lane);
				policy.ChargeFifo(std::chrono::nanoseconds{1});
				break;
			case Lane::Priority:
				++counts.priority;
				policy.CommitTake(lane);
				policy.ChargePriority(std::chrono::nanoseconds{1});
				break;
			case Lane::None:
				++counts.none;
				break;
		}
	}

	return counts;
}
} // namespace

void hbe::StreamDrainPolicyTest::Prepare()
{
	AddTest("No work in either lane yields None", [this](auto& ls)
	{
		StreamDrainPolicy policy;
		policy.ConfigureRate(1, 1);

		if (policy.ChooseLane(false, false) != Lane::None)
		{
			ls << "A stream with nothing queued was told to take work." << lferr;
		}
	});

	AddTest("Unlimited allowance serves the configured ratio", [this](auto& ls)
	{
		StreamDrainPolicy policy;
		policy.ConfigureRate(3, 1);

		const auto counts = DriveTakes(policy, 8);
		ls << "Over 8 takes at a 3:1 rate: fifo=" << counts.fifo << " priority=" << counts.priority << lf;

		if (counts.fifo != 6 || counts.priority != 2)
		{
			ls << "A 3:1 rate produced " << counts.fifo << ":" << counts.priority << " over 8 takes, not 6:2." << lferr;
		}

		if (counts.none != 0)
		{
			ls << "An unlimited stream refused work " << counts.none << " times." << lferr;
		}
	});

	AddTest("An empty lane does not hold its share hostage", [this](auto& ls)
	{
		StreamDrainPolicy policy;
		policy.ConfigureRate(1, 1);
		policy.ConfigureAllowance(std::chrono::duration<double, std::milli>{1});

		policy.ChargePriority(
				std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double, std::milli>{0.6}));

		if (policy.ChooseLane(false, true) != Lane::Priority)
		{
			ls << "The only lane with work was refused because it had passed its own share, leaving the stream idle."
			   << lferr;
		}

		if (policy.IsRoundExhausted())
		{
			ls << "0.6 ms of 1 ms allowance reported the round exhausted." << lferr;
		}
	});

	AddTest("Both shares spent exhausts the round", [this](auto& ls)
	{
		StreamDrainPolicy policy;
		policy.ConfigureRate(1, 1);
		policy.ConfigureAllowance(std::chrono::duration<double, std::milli>{1});

		const auto overspill =
				std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double, std::milli>{0.6});
		policy.ChargeFifo(overspill);
		policy.ChargePriority(overspill);

		if (!policy.IsRoundExhausted())
		{
			ls << "1.2 ms charged against a 1 ms allowance did not exhaust the round." << lferr;
		}

		if (policy.ChooseLane(true, true) != Lane::None)
		{
			ls << "An exhausted round still offered a lane." << lferr;
		}

		policy.EndRound();

		if (policy.IsRoundExhausted() || policy.ChooseLane(true, true) == Lane::None)
		{
			ls << "EndRound did not reopen the shares." << lferr;
		}
	});

	AddTest("A light lane is never starved by a heavy one", [this](auto& ls)
	{
		StreamDrainPolicy policy;
		policy.ConfigureRate(1, 3);

		const auto counts = DriveTakes(policy, 8);
		ls << "Over 8 takes at a 1:3 rate: fifo=" << counts.fifo << " priority=" << counts.priority << lf;

		if (counts.fifo < 1)
		{
			ls << "The lighter lane was starved over 8 takes at 1:3 (fifo=" << counts.fifo << ")." << lferr;
		}

		if (counts.priority < counts.fifo)
		{
			ls << "The heavier lane ran less often than the lighter one at 1:3." << lferr;
		}
	});

	AddTest("Zero weights fall back to an even split", [this](auto& ls)
	{
		StreamDrainPolicy policy;
		policy.ConfigureRate(0, 0);

		const auto counts = DriveTakes(policy, 4);
		ls << "Over 4 takes with unset weights: fifo=" << counts.fifo << " priority=" << counts.priority << lf;

		if (counts.fifo != 2 || counts.priority != 2)
		{
			ls << "Unset weights gave " << counts.fifo << ":" << counts.priority
			   << " over 4 takes instead of an even split." << lferr;
		}
	});

	AddTest("Shares add up to the allowance", [this](auto& ls)
	{
		StreamDrainPolicy policy;
		policy.ConfigureRate(3, 1);
		policy.ConfigureAllowance(std::chrono::duration<double, std::milli>{2});

		const auto fifoMicros = std::chrono::duration_cast<std::chrono::microseconds>(policy.GetFifoShare()).count();
		const auto priorityMicros =
				std::chrono::duration_cast<std::chrono::microseconds>(policy.GetPriorityShare()).count();
		ls << "At 3:1 over a 2 ms allowance: fifo=" << fifoMicros << "us priority=" << priorityMicros << "us" << lf;

		if (fifoMicros != 1500 || priorityMicros != 500)
		{
			ls << "3:1 over 2 ms gave " << fifoMicros << "/" << priorityMicros << " us, not 1500/500." << lferr;
		}
	});

	AddTest("A stream with no max age ages nothing out, and work it cannot date is never judged", [this](auto& ls)
	{
		using namespace std::chrono_literals;

		StreamDrainPolicy unlimited;
		unlimited.ConfigureRate(1, 1);

		if (unlimited.IsOverAge(std::chrono::nanoseconds{1}, std::chrono::nanoseconds{10s}))
		{
			ls << "An unlimited stream aged out work 10s old; zero must mean unlimited, not 'drop everything'" << lferr;

			return;
		}

		StreamDrainPolicy bounded;
		bounded.ConfigureRate(1, 1);
		bounded.SetMaxAge(20ms);

		if (bounded.GetMaxAge() != 20ms)
		{
			ls << "SetMaxAge(20ms) reports " << bounded.GetMaxAge().count() << "ns" << lferr;

			return;
		}

		if (bounded.IsOverAge(std::chrono::nanoseconds{0}, std::chrono::nanoseconds{10s}))
		{
			ls << "An unstamped item was aged out; a stream may not invent an age for work it cannot date" << lferr;

			return;
		}

		if (!bounded.IsOverAge(std::chrono::nanoseconds{1}, std::chrono::nanoseconds{21ms}))
		{
			ls << "Work 21ms old survived a 20ms ceiling" << lferr;

			return;
		}

		if (bounded.IsOverAge(1ms, 21ms))
		{
			ls << "Work exactly 20ms old survived out a 20ms ceiling; the comparison must be strict" << lferr;

			return;
		}

		if (!bounded.IsOverAge(1ms, 21ms + 1ns))
		{
			ls << "Work 20ms and 1ns old survived a 20ms ceiling" << lferr;

			return;
		}
	});
}

#endif //__TEST__
