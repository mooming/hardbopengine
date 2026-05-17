// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

namespace hbe
{

	/// @brief A marker class for inserting newlines in string builders.
	class EndLine final
	{
	public:
		operator const char*() const { return "\n"; }
	};

	static constexpr EndLine hendl;

} // namespace hbe
