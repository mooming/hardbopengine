// Created by mooming.go@gmail.com, 2022

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
