// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Constants_h
#define Constants_h

namespace HE
{
	constexpr size_t KB = 1024;
	constexpr size_t MB = 1024 * 1024;
	constexpr size_t GB = 1024 * 1024 * 1024;

    constexpr float Pi = 3.141592653589793f;
    constexpr float HalfPi = Pi * 0.5f;
    constexpr float TwoPi = Pi * 2.0f;
    constexpr float EulerNum = 2.718281828459f;

    constexpr float Epsilon = 0.001f;
    constexpr float SqrEpsilon = Epsilon * Epsilon;

	namespace Physics
	{
		constexpr float Epsilon = 0.005f;
		constexpr float SqrEpsilon = Epsilon * Epsilon;
	}
}

#endif /* Constants_h */
