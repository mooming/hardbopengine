// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "String/StaticString.h"

namespace OS
{

	/// @brief Get a stack backtrace for debugging purposes.
	hbe::StaticString GetBackTrace(uint16_t startIndex = 0, uint16_t maxDepth = 512);

} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	/// @brief Test collection for OS debug operations.
	class OSDebugTest : public TestCollection
	{
	public:
		inline OSDebugTest() : TestCollection("OSDebugTest") {}

	protected:
		virtual void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
