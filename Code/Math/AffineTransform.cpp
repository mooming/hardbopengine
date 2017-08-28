#include "AffineTransform.h"

namespace HE
{
    template class AffineTransform<float>;
}

#ifdef __UNIT_TEST__

#include "Matrix4x4.h"

#include "System/Time.h"
#include "System/Vector.h"
#include <iostream>

bool HE::AffineTransformTest::DoTest()
{
    using namespace std;


    return true;
}

#endif //__UNIT_TEST__
