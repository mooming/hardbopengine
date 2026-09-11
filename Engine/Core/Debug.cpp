// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Debug.h"

#include "../Engine/Engine.h"
#include "Log/Logger.h"


namespace hbe
{
void flushLogs()
{
	auto& engine = Engine::get();
	engine.flushLog();
}
} // namespace hbe
