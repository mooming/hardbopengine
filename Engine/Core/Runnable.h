// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>


namespace hbe
{
/// API reference: docs/Core/index.html#runnable
using TRunnable = std::size_t (*)(void*, std::size_t, std::size_t);
} // namespace hbe
