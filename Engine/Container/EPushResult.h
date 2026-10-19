// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


namespace hbe
{
/// API reference: docs/Container/EPushResult/index.html
enum class EPushResult : unsigned char
{
	Pushed,
	Full,
	TooManyProducers
};
} // namespace hbe
