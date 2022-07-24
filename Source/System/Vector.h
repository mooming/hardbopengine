// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#pragma once

#include "HardbopAllocator.h"

#include <vector>

namespace HE
{
	template <typename T>
	using Vector = std::vector<T, HardbopAllocator<T>>;
}

#ifdef __UNIT_TEST__
namespace HE
{

	class VectorTest : public TestCase
	{
	public:

		VectorTest() : TestCase("VectorTest")
		{
		}

	protected:
		virtual bool DoTest() override;
	};
}
#endif //__UNIT_TEST__
