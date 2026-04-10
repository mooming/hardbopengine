// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once
#include <cstddef>
#include <cstdint>
#include "String/StaticString.h"

namespace hbe
{

	/// @brief Handle to a loaded resource managed by ResourceManager.
	class Resource final
	{
	public:
		Resource();
		~Resource();
	};

} // namespace hbe
