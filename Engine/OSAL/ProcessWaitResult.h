// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


namespace OS
{
enum class EProcessWaitResult : unsigned char
{
	Exited,
	TimedOut,
	Failed
};
} // namespace OS
