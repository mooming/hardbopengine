// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "PipelineTypes.h"


namespace hbe::pipeline
{
class Batch final
{
public:
	const TypeID typeID = 0;
	const RequestID requestID = 0;
	void* const data = nullptr;
	const TSize count = 0;

public:
	[[nodiscard]] bool IsValid() const noexcept { return data != nullptr;}
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
