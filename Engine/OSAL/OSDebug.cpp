// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSDebug.h"

#ifdef TEST_ENABLED

namespace hbe
{
void OSDebugTest::Prepare()
{
	AddTest("Print CallStack", [this](auto& ls)
	{
		auto callstack = OS::GetBackTrace();
		ls << callstack.c_str() << lf;
	});
}
} // namespace hbe
#endif //TEST_ENABLED
