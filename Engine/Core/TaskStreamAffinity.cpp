// Created by Hansol Park (mooming.go@gmail.com)
// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TaskStreamAffinity.h"

#include <algorithm>


void hbe::TaskStreamAffinityTest::Prepare()
{
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
			if ( (i%2) == 0)
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
