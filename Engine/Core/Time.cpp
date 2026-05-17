// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Time.h"

#include <chrono>
#include <thread>


namespace hbe
{

void time::Sleep(time::TMilliSec milli) noexcept
{
using namespace std::chrono;
std::this_thread::sleep_for(std::chrono::milliseconds(milli));
}

} // namespace hbe
