// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "ScopedLock.h"

#include <mutex>

namespace hbe
{
	template class ScopedLock<std::mutex>;
} // namespace hbe
