// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


namespace hbe
{
/// API reference: docs/String/EndLine/index.html
class EndLine final
{
public:
	[[nodiscard]] operator const char*() const
	{
		return "\n";
	}
};

static constexpr EndLine hendl;
} // namespace hbe
