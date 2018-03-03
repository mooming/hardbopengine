//
//  StdUtil.cpp
//  Code
//
//  Created by mooming on 2018. 3. 3..
//
//

#include "StdUtil.h"

#include "Constants.h"

#include <cmath>

namespace HE
{
    bool Equals(float a, float b)
    {
        return std::fabs(b - a) < Epsilon;
    }

    bool Equals(double a, double b)
    {
        return std::abs(b - a) < Epsilon;
    }
}
