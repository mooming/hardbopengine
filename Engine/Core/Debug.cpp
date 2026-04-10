

#include "Debug.h"

#include "../Engine/Engine.h"
#include "Log/Logger.h"

namespace hbe
{
	void FlushLogs()
	{
		auto& engine = Engine::Get();
		engine.FlushLog();
	}
} // namespace hbe
