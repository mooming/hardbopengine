// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __UNIT_TEST__
#include "RendererTest.h"

#include "RendererCommon.h"


namespace hbe
{

RendererTest::RendererTest() noexcept
	:TestCollection("RendererTest")
{
}

void RendererTest::Prepare()
{
	AddTest("RHICapabilities", [](auto& log)
	{

	});

	AddTest("GetPreferredAPI", [](auto& log)
	{

	});

	AddTest("RendererFactory", [](auto& log)
	{
	});
}

} // namespace hbe
#endif // __UNIT_TEST__
