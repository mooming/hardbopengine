// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSDebug.h"

#ifdef __UNIT_TEST__

namespace hbe
{

void OSDebugTest::prepare()
{
	addTest("Print CallStack", [this](auto& ls)
	{
		auto callstack = OS::getBackTrace();
		ls << callstack.c_str() << lf;
	});
}

} // namespace hbe
#endif //__UNIT_TEST__
