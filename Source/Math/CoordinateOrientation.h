// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef CoordinateOrientation_h
#define CoordinateOrientation_h

#include "System/Platform.h"

#ifdef __LEFT_HANDED__
#ifdef __RIGHT_HANDED__

ERROR: SHOULD NOT DEFINE BOTH HANDEDNESS!

#endif //__RIGHT_HANDED__
#endif //__LEFT_HANDED__

#include <cmath>

namespace HE
{
#ifdef __LEFT_HANDED__
    inline float RotationSin(float radian) { return -sinf(radian); }
    inline float RotationCos(float radian) { return cosf(radian); }
    inline float RotationTan(float radian) { return -tanf(radian); }
    inline float RotationAsin(float radian) { return -asinf(radian); }
    inline float RotationAcos(float radian) { return acosf(radian); }
    inline float RotationAtan2(float y, float x) { return -atan2f(y, x); }
#endif //__LEFT_HANDED__

#ifdef __RIGHT_HANDED__
    inline float RotationSin(float radian) { return sinf(radian); }
    inline float RotationCos(float radian) { return cosf(radian); }
    inline float RotationTan(float radian) { return tanf(radian); }
    inline float RotationAsin(float radian) { return asinf(radian); }
    inline float RotationAcos(float radian) { return acosf(radian); }
    inline float RotationAtan2(float y, float x) { return atan2f(y, x); }
#endif //__RIGHT_HANDED__
}

#endif //CoordinateOrientation_h
