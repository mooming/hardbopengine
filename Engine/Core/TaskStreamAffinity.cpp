// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __TEST__
#include "TaskStreamAffinity.h"

#include <algorithm>
#include <array>

void hbe::TaskStreamAffinityTest::Prepare()
{
	AddTest("A mask reserves one word per 64 bits and not one per 8", [this](TLogOut& ls)
	{
		constexpr std::size_t bitsPerWordBytes = sizeof(std::uint64_t);
		constexpr std::size_t bitsPerWord = bitsPerWordBytes * 8;

		if (sizeof(TaskStreamAffinity) != bitsPerWordBytes)
		{
			ls << "A mask of " << TaskStreamAffinity::GetNumBits() << " bits is " << sizeof(TaskStreamAffinity)
			   << " bytes, so the word count is not derived from bits per word. A queued work item embeds this type, so"
			   << " every byte here is paid once per item per lane change." << lferr;
		}

		constexpr std::size_t wideBits = 517;
		using WideMask = TaskStreamAffinityBase<wideBits>;
		constexpr std::size_t expectedWideBytes = ((wideBits + bitsPerWord - 1) / bitsPerWord) * bitsPerWordBytes;

		if (sizeof(WideMask) != expectedWideBytes)
		{
			ls << "A mask of " << wideBits << " bits is " << sizeof(WideMask) << " bytes, not " << expectedWideBytes
			   << ". Rounding up to a whole word is the whole contract, and the previous formula"
			   << " rounded up to a whole word count instead." << lferr;
		}
	});

	AddTest("Taking away one stream leaves every other stream alone", [this](TLogOut& ls)
	{
		constexpr unsigned int wideBits = 517;
		using WideMask = TaskStreamAffinityBase<wideBits>;

		constexpr std::array<unsigned int, 7> probes = {0U, 1U, 63U, 64U, 127U, 128U, wideBits - 1};

		for (const unsigned int probe : probes)
		{
			WideMask affinity;
			affinity.Unset(probe);

			if (affinity.Get(probe))
			{
				ls << "Stream " << probe << " still reads as allowed after being taken away." << lferr;
			}

			unsigned int disturbed = 0;
			for (unsigned int bit = 0; bit < wideBits; ++bit)
			{
				if (bit != probe && !affinity.Get(bit))
				{
					++disturbed;
				}
			}

			if (disturbed != 0)
			{
				ls << "Taking away stream " << probe << " also took away " << disturbed
				   << " other stream(s). Two streams sharing a bit is not a style problem: a task barred from one "
					  "stream"
				   << " would be barred from whichever stream the collision landed on, and the pair depends on the word"
				   << " index arithmetic." << lferr;
			}
		}

		WideMask pastEnd;
		pastEnd.Unset(wideBits);
		pastEnd.Unset(wideBits + 1000);
		for (unsigned int bit = 0; bit < wideBits; ++bit)
		{
			if (!pastEnd.Get(bit))
			{
				ls << "An index past the mask's last stream (" << wideBits
				   << ") disturbed a real stream, so the guard is not holding and padding bits in the last word are"
				   << " reachable." << lferr;
				break;
			}
		}
	});

	AddTest("Default Constructor", [this](TLogOut& ls)
	{
		TaskStreamAffinityBase<517> affinity;

		for (unsigned int i = 0; i < affinity.GetNumBits(); ++i)
		{
			const bool affinityValue = affinity.Get(i);
			if (!affinityValue)
			{
				ls << "Default constructed affinity shouldn't be false at index " << i << lferr;
			}
		}
	});

	AddTest("Affinity Unset", [this](TLogOut& ls)
	{
		TaskStreamAffinityBase<517> affinity;

		for (unsigned int i = 0; i < affinity.GetNumBits(); ++i)
		{
			affinity.Unset(i);
		}
		{
			constexpr unsigned int PrintCount = 3;
			const auto numBits = affinity.GetNumBits();
			for (unsigned int i = 0; i < std::min(PrintCount, numBits); ++i)
			{
				ls << i << ": affinity " << affinity.Get(i) << lf;
			}

			if (numBits > PrintCount * 2)
			{
				ls << "... " << (numBits - PrintCount * 2) << " similar lines omitted ..." << lf;
			}

			for (unsigned int i = std::max(PrintCount, numBits - PrintCount); i < numBits; ++i)
			{
				ls << i << ": affinity " << affinity.Get(i) << lf;
			}
		}

		for (unsigned int i = 0; i < affinity.GetNumBits(); ++i)
		{
			const bool affinityValue = affinity.Get(i);
			if (affinityValue)
			{
				ls << "Unset failed at " << i << lferr;
			}
		}
	});

	AddTest("Affinity Set/Unset", [this](TLogOut& ls)
	{
		TaskStreamAffinityBase<517> affinity;

		for (unsigned int i = 0; i < affinity.GetNumBits(); ++i)
		{
			if ((i % 2) == 0)
			{
				affinity.Unset(i);
			}
			else
			{
				affinity.Set(i);
			}
		}
		{
			constexpr unsigned int PrintCount = 3;
			const auto numBits = affinity.GetNumBits();
			for (unsigned int i = 0; i < std::min(PrintCount, numBits); ++i)
			{
				ls << i << ": affinity " << affinity.Get(i) << lf;
			}

			if (numBits > PrintCount * 2)
			{
				ls << "... " << (numBits - PrintCount * 2) << " similar lines omitted ..." << lf;
			}

			for (unsigned int i = std::max(PrintCount, numBits - PrintCount); i < numBits; ++i)
			{
				ls << i << ": affinity " << affinity.Get(i) << lf;
			}
		}

		for (unsigned int i = 0; i < affinity.GetNumBits(); ++i)
		{
			const bool affinityValue = affinity.Get(i);
			if ((i % 2) == 0)
			{
				if (affinityValue)
				{
					ls << "Unset failed at " << i << lferr;
				}
			}
			else
			{
				if (!affinityValue)
				{
					ls << "Set failed at " << i << lferr;
				}
			}
		}
	});
}
#endif // __TEST__
