// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Memory/BaseAllocator.h"
#include <vector>


namespace HE
{
	template <typename T>
	using Vector = std::vector<T, BaseAllocator<T>>;
} // HE

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
