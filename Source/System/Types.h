// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Types_h
#define Types_h

#include <cstddef>
#include <cstdint>

namespace HE
{
	using Byte = uint8_t;
	using Pointer = void*;
	using Index = size_t;

	using Int = int32_t;
	using UInt = uint32_t;

	using Real = float;

    using AllocatorId = uint8_t;
}

#endif /* Types_h */
