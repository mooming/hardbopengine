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

    AffineTRS trs;

    for (int z = 0; z < 180; ++z)
    {
        for (int y = 0; y < 180; ++y)
        {
            for (int x = 0; x < 180; ++x)
            {
                auto vec = Float3(x + 1, y + 1, z + 1);
                vec.Normalize();

                auto rot = Quat(x, y, z);

                trs.SetRotationScale(rot, vec * 3.0f);
                auto trsRot = trs.Rotation();

                if ((rot * vec) != (trsRot * vec))
                {
                    cout << '(' << x << ", " << y << ", " << z << ") "
                        << trsRot.EulerAngles() << endl;
                    cerr << "[Error] Rotation mismatched. "
                        << (trsRot * vec) << ", but expected "
                        << (rot * vec) << endl;

                    return false;
                }
            }
        }
    }

    return true;
}

#endif //__UNIT_TEST__
