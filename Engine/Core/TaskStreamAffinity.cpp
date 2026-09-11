// Created by Hansol Park (mooming.go@gmail.com)
// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __UNIT_TEST__
#include "TaskStreamAffinity.h"

#include <algorithm>


void hbe::TaskStreamAffinityTest::prepare()
{
	addTest("Default Constructor", [this](TLogOut& ls)
	{
		TaskStreamAffinityBase<517> affinity;

		for (unsigned int i = 0; i < affinity.getNumBits(); ++i)
		{
			const bool affinityValue = affinity.get(i);
			if (!affinityValue)
			{
				ls << "Default constructed affinity shouldn't be false at index " << i << lferr;
			}
		}
	});

	addTest("Affinity Unset", [this](TLogOut& ls)
	{
		TaskStreamAffinityBase<517> affinity;

		for (unsigned int i = 0; i < affinity.getNumBits(); ++i)
		{
			affinity.unset(i);
		}

		{
			constexpr unsigned int PrintCount = 3;
			const auto numBits = affinity.getNumBits();
			for (unsigned int i = 0; i < std::min(PrintCount, numBits); ++i)
			{
				ls << i << ": affinity " << affinity.get(i) << lf;
			}
			if (numBits > PrintCount * 2)
			{
				ls << "... " << (numBits - PrintCount * 2) << " similar lines omitted ..." << lf;
			}
			for (unsigned int i = std::max(PrintCount, numBits - PrintCount); i < numBits; ++i)
			{
				ls << i << ": affinity " << affinity.get(i) << lf;
			}
		}

		for (unsigned int i = 0; i < affinity.getNumBits(); ++i)
		{
			const bool affinityValue = affinity.get(i);
			if (affinityValue)
			{
				ls << "Unset failed at " << i << lferr;
			}
		}
	});

	addTest("Affinity Set/Unset", [this](TLogOut& ls)
	{
		TaskStreamAffinityBase<517> affinity;

		for (unsigned int i = 0; i < affinity.getNumBits(); ++i)
		{
			if ( (i%2) == 0)
			{
				affinity.unset(i);
			}
			else
			{
				affinity.set(i);
			}
		}

		{
			constexpr unsigned int PrintCount = 3;
			const auto numBits = affinity.getNumBits();
			for (unsigned int i = 0; i < std::min(PrintCount, numBits); ++i)
			{
				ls << i << ": affinity " << affinity.get(i) << lf;
			}
			if (numBits > PrintCount * 2)
			{
				ls << "... " << (numBits - PrintCount * 2) << " similar lines omitted ..." << lf;
			}
			for (unsigned int i = std::max(PrintCount, numBits - PrintCount); i < numBits; ++i)
			{
				ls << i << ": affinity " << affinity.get(i) << lf;
			}
		}

		for (unsigned int i = 0; i < affinity.getNumBits(); ++i)
		{
			const bool affinityValue = affinity.get(i);
			if ( (i%2) == 0)
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
#endif // __UNIT_TEST__
