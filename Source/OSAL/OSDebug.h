// Created by mooming.go@gmail.com 2022

#pragma once

#include "Config/BuildConfig.h"
#include "String/StaticString.h"

namespace OS
{

	hbe::StaticString GetBackTrace(uint16_t startIndex = 0, uint16_t maxDepth = 512);

} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class OSDebugTest : public TestCollection
	{
	public:
		inline OSDebugTest() : TestCollection("OSDebugTest") {}

	protected:
		virtual void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
