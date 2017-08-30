#include "RigidTransform.h"

using namespace HE;

#ifdef __UNIT_TEST__

#include "Matrix4x4.h"

#include "System/Time.h"
#include "System/Vector.h"
#include <iostream>

bool RigidTransformTest::DoTest()
{
    using namespace std;

    {
        RigidTR tm;
        cout << tm << endl;

        if (tm != tm)
        {
            cerr << "Self equality failed" << endl;

            return false;
        }

        if (tm.rotation != Quat::Identity)
        {
            cerr << "The default constructor of RigidTR should have "
                << "the identity rotation." << endl;

            return false;
        }

        if (tm.translation != Float3::Zero)
        {
            cerr << "The default constructor of RigidTR should have "
                << "the zero translation." << endl;

            return false;
        }

        Float4x4 mat = tm.ToMatrix();
        if (mat != Float4x4::Identity)
        {
            cerr << "The default constructor of RigidTR should be "
                << "equal to the identity matrix." << endl;

            return false;
        }

        if (tm * Float3::Forward != Float3::Forward)
        {
            cerr << "Identity Transform Failed. " << (tm * Float3::Forward)
                << ", but " << Float3::Forward << " expected." << endl;

            return false;
        }
    }

    {
        RigidTR tm(Float3(4, 5, 6), Quat(0, 47, 0));
        RigidTR tm2(Float3(3, 4, 5), Quat(41, 0, 25));
        RigidTR tm3 = tm * tm2;

        cout << tm << endl;
        cout << tm2 << endl;
        cout << tm3 << endl;

        Float3 result1 = tm3 * Float3::Forward;
        Float3 result2 = tm * (tm2 * Float3::Forward);
        if (result1 != result2)
        {
            cerr << "Transform result failed. " << (tm3 * Float3::Forward)
                << ", but " << result2 << " expected." << endl;
            return false;
        }

        if (!tm3.ToMatrix().IsInvertible())
        {
            cerr << "Transform matrix should be invertible." << endl
                << tm3 << endl;

            return false;
        }

        Float3 result = tm3.Inverse() * result1;
        if (result != Float3::Forward)
        {
            cerr << "Transform matrix inverse failed. " << result
                << ", but " << Float3::Forward << " expected." << endl;

            return false;
        }

        result = tm2.InverseTransform(tm.InverseTransform(result2));
        if (result != Float3::Forward)
        {
            cerr << "Inverse transform failed. " << result
                << ", but " << Float3::Forward << " expected." << endl;

            return false;
        }

        if (tm3.Inverse().Inverse() != tm3)
        {
            cerr << "Inverse of inverse transform failed. " << tm3.Inverse().Inverse()
                << ", but " << tm3 << " expected." << endl;

            return false;
        }
    }

    return true;
}

#endif //__UNIT_TEST__
