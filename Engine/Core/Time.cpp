// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Time.h"

#include <chrono>
#include <thread>

using namespace std;
using namespace hbe;

void time::Sleep(time::MilliSec milli)
{
	using namespace std::chrono;
	this_thread::sleep_for(milliseconds(milli));
}
