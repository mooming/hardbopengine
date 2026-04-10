// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSThread.h"

#include <thread>
#include "../Engine/Engine.h"

void OS::Yield() { std::this_thread::yield(); }

void OS::Sleep(uint32_t milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); }

#ifdef __UNIT_TEST__

void hbe::OSThreadTest::Prepare()
{
	// TODO
}

#endif //__UNIT_TEST__
