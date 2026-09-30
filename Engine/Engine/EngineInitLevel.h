// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>

namespace hbe
{
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
