// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>


namespace hbe::pipeline
{
using TypeID = std::uintptr_t;
using RequestID = uint64_t;

class Batch final
{
public:
	const TypeID typeID = 0;
	const RequestID requestID = 0;
	void* data = nullptr;
	std::size_t count = 0;
};
} // namespace hbe::pipeline

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe::pipeline
{
class BatchTest final : public TestCollection
{
public:
	BatchTest()
		: TestCollection("BatchTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe::pipeline
#endif // TEST_ENABLED
