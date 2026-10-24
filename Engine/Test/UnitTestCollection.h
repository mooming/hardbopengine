// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#ifdef TEST_ENABLED

#include <cstddef>

namespace hbe
{
namespace Test
{
void RegisterSuite();

void ScheduleSuiteOnBaseStream();

void RegisterPerfBenchmarkSuite();
} // namespace Test
} // namespace hbe

#endif // TEST_ENABLED
