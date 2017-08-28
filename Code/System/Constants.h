// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Constants_h
#define Constants_h

namespace HE
{
	constexpr size_t KB = 1024;
	constexpr size_t MB = 1024 * 1024;
	constexpr size_t GB = 1024 * 1024 * 1024;

    constexpr float pi = 3.141592653589793f;
    constexpr float halfPi = pi * 0.5f;
    constexpr float twoPi = pi * 2.0f;
    constexpr float e = 2.718281828459f;

    constexpr float epsilon = 0.001f;
    constexpr float sqrEpsilon = epsilon * epsilon;

	namespace Physics
	{
		constexpr float epsilon = 0.005f;
		constexpr float sqrEpsilon = epsilon * epsilon;
	}
}

#endif /* Constants_h */
