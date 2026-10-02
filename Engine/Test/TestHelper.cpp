// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TestHelper.h"

#include "Core/TaskSystem.h"
#include "Log/Logger.h"

namespace hbe
{
namespace TestHelper
{

void ReportDriveTimeout(TaskSystem& taskSystem, const char* waitingFor,
						const std::chrono::milliseconds patience) noexcept
{
	Logger::Get().AddLog(taskSystem.GetName(), ELogLevel::Error, [waitingFor, patience](auto& logStream)
	{
		logStream << "A thread driving the engine loop gave up waiting for " << waitingFor << " after "
				  << patience.count()
				  << "ms. The work was still queued, so nothing was going to run it, and a caller that proceeds "
					 "from here "
					 "usually blocks on the very thing it waited for. The intake that should have drained it is "
					 "the first "
					 "thing to look at.";
	});
}

} // namespace TestHelper
} // namespace hbe
