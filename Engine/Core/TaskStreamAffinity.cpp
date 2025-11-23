// Created by Hansol Park (mooming.go@gmail.com)

#include "TaskStreamAffinity.h"


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

		for (unsigned int i = 0; i < affinity.GetNumBits(); ++i)
		{
			const bool affinityValue = affinity.Get(i);
			ls << i << ": affinity " << affinityValue << lf;
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

		for (unsigned int i = 0; i < affinity.GetNumBits(); ++i)
		{
			const bool affinityValue = affinity.Get(i);
			ls << i << ": affinity " << affinityValue << lf;
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
