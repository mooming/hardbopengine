// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include <ostream>

namespace hbe
{

	/// @brief Enumeration representing the lifecycle states of a component.
	enum class ComponentState : int
	{
		NONE,
		BORN,
		ALIVE,
		SLEEP,
		DEAD
	};

	inline std::ostream& operator<<(std::ostream& os, const ComponentState& state)
	{
		static const char* names[] = {"NONE", "BORN", "ALIVE", "SLEEP", "DEAD"};

		os << names[static_cast<int>(state)];
		return os;
	}
} // namespace hbe
