// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifndef __HBE_TEST_RING_BUFFER_PERF__
#define __HBE_TEST_RING_BUFFER_PERF__

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
/// API reference: docs/Test/RingBufferPerfTest/index.html
class RingBufferPerfTest final : public TestCollection
{
public:
	RingBufferPerfTest();

	void Prepare() override;
};

/// API reference: docs/Test/RingBufferPerfBenchmarkTest/index.html
class RingBufferPerfBenchmarkTest final : public TestCollection
{
public:
	RingBufferPerfBenchmarkTest();

	void Prepare() override;
};
} // namespace hbe

#endif // __UNIT_TEST__

#endif // __HBE_TEST_RING_BUFFER_PERF__
