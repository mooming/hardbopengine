// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>

namespace hbe
{
using TByte = uint8_t;
using TPointer = void*;
using TIndex = size_t;

using TInt = int32_t;
using TUInt = uint32_t;

using TReal = float;

// Backward-compatible aliases
using Byte = TByte;
using Pointer = TPointer;
using Index = TIndex;
using Int = TInt;
using UInt = TUInt;
using Real = TReal;
} // namespace hbe
