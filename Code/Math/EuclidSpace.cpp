#include "EuclidSpace.h"

namespace HE
{
    template class EuclidSpace<float>;
}

#ifdef __UNIT_TEST__
#include <iostream>

bool HE::EuclidSpaceTest::DoTest()
{
    using namespace std;


    return true;
}

#endif //__UNIT_TEST__
