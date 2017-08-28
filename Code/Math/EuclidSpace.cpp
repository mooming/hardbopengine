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
    
    FloatSpace root;
    cout << "Root = " << root << endl;
    
    root.SetPosition({0.0f, 1.0f, 2.0f});
    cout << "Moved Root = " << root << endl;
    
    return true;
}

#endif //__UNIT_TEST__
