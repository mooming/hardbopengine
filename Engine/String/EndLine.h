// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

namespace hbe
{

	/// @brief A marker struct for inserting newlines in string builders.
	struct EndLine final
	{
		operator const char*() const { return "\n"; }
	};

	static constexpr EndLine hendl;

} // namespace hbe
