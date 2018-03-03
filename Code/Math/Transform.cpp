//
//  Transform.cpp
//  Code
//
//  Created by mooming on 2018. 3. 3..
//
//

#include "Transform.h"


#ifdef __UNIT_TEST__

#include "Vector3.h"
#include "../System/StdUtil.h"

using namespace HE;

bool TransformTest::DoTest()
{
    using namespace std;

    FTransform root;
    cout << root << endl;

    {
        cout << "Test Hierachycal Translation" << endl;
        FTransform a(root);
        FTransform b(a);
        FTransform c(b);

        a.Set(Float3(0, 0, 1));
        b.Set(Float3(0, 1, 0));
        c.Set(Float3(1, 0, 0));

        cout << a << endl;
        cout << b << endl;
        cout << c << endl;

        if (c.GetWorldTransform().translation != Float3(1, 1, 1))
            return false;
    }


    {
        cout << "Test Hierachycal Rotation" << endl;
        FTransform a(root);
        FTransform b(a);
        b.Set(Float3(0, 0, 1));
        a.Set(Quat(0.0f, 45.0f, 0.0f));

        cout << b << endl;
        if (!b.GetWorldTransform().translation.IsUnity())
            return false;

        a.Set(Quat(0.0f, 90.0f, 0.0f));

        cout << b << endl;

        if (b.GetWorldTransform().translation != Float3(1, 0, 0))
            return false;
    }

    {
        cout << "Test Hierachycal Rotation 2" << endl;
        FTransform a(root);
        FTransform b(a);
        FTransform c(b);
        a.Set(Quat(0.0f, 45.0f, 0.0f));
        b.Set(Quat(0.0f, 45.0f, 0.0f));
        c.Set(Float3(0, 0, 1));

        cout << c << endl;
        if (!c.GetWorldTransform().translation.IsUnity())
            return false;

        cout << c << endl;

        if (c.GetWorldTransform().translation != Float3(1, 0, 0))
            return false;
    }

    return true;
}

#endif //__UNIT_TEST__
