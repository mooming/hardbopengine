// Created by mooming.go@gmail.com, 2017

#include "Time.h"

#include <chrono>
#include <thread>

using namespace std;
using namespace hbe;

void Time::Sleep(Time::MilliSec milli)
{
	using namespace std::chrono;
	this_thread::sleep_for(milliseconds(milli));
}
