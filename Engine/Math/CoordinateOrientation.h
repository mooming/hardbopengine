// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"

#ifdef __LEFT_HANDED__
#ifdef RIGHT_HANDED_COORDINATE

static_assert(false, "SHOULD NOT DEFINE BOTH HANDEDNESS !");

#endif //RIGHT_HANDED_COORDINATE
#endif //__LEFT_HANDED__

#include <cmath>

namespace hbe
{
#ifdef __LEFT_HANDED__
	inline float rotationSin(float radian) { return -sinf(radian); }
	inline float rotationCos(float radian) { return cosf(radian); }
	inline float rotationTan(float radian) { return -tanf(radian); }
	inline float rotationAsin(float radian) { return -asinf(radian); }
	inline float rotationAcos(float radian) { return acosf(radian); }
	inline float rotationAtan2(float y, float x) { return -atan2f(y, x); }
#endif //__LEFT_HANDED__

#ifdef RIGHT_HANDED_COORDINATE
	inline float rotationSin(float radian) { return sinf(radian); }
	inline float rotationCos(float radian) { return cosf(radian); }
	inline float rotationTan(float radian) { return tanf(radian); }
	inline float rotationAsin(float radian) { return asinf(radian); }
	inline float rotationAcos(float radian) { return acosf(radian); }
	inline float rotationAtan2(float y, float x) { return atan2f(y, x); }
#endif //RIGHT_HANDED_COORDINATE
} // namespace hbe
