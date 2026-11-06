// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class OSProcessTest final : public TestCollection
{
public:
	OSProcessTest()
		: TestCollection("OSProcessTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif // TEST_ENABLED
