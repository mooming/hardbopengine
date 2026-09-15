// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>

namespace hbe
{
/// @brief Which engine subsystems Engine::Initialize should start.
/// @details A tool that converts one file into another needs neither a window server nor a resource
///          manager, and starting them anyway is not merely wasteful: the Application level creates
///          the OS application object and connects to the window server, which a headless or CI
///          context may refuse. Levels are flags, so a caller states exactly the set it needs.
/// @note Logger implies TaskSystem - the logger writes through the task system's IO stream - and
///       Engine::Initialize adds the dependency rather than trusting the caller to spell it out.
/// @note A level decides what is *started*, never what exists. MemoryManager, SystemStatistics and
///       the Logger object itself are members of Engine and are constructed before this is ever
///       called; what they need at that point is a live MemoryManager, which is why the levels
///       cannot be used to omit memory.
enum class EInitLevel : uint8_t
{
	None = 0,
	TaskSystem = 1 << 0,
	Logger = 1 << 1,
	Application = 1 << 2,

	All = TaskSystem | Logger | Application
};

constexpr EInitLevel operator|(EInitLevel lhs, EInitLevel rhs) noexcept
{
	return static_cast<EInitLevel>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr bool HasFlag(EInitLevel levels, EInitLevel flag) noexcept
{
	return (static_cast<uint8_t>(levels) & static_cast<uint8_t>(flag)) == static_cast<uint8_t>(flag);
}

static_assert(static_cast<uint8_t>(EInitLevel::None) == 0, "None must select nothing at all");
static_assert(static_cast<uint8_t>(EInitLevel::All) ==
					  (static_cast<uint8_t>(EInitLevel::TaskSystem) | static_cast<uint8_t>(EInitLevel::Logger) |
					   static_cast<uint8_t>(EInitLevel::Application)),
			  "All must be exactly the union of the levels, so adding a level cannot leave it out");
} // namespace hbe
