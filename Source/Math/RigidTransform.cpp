#include "RigidTransform.h"

using namespace HE;

#ifdef __UNIT_TEST__
#include "Matrix4x4.h"
#include "System/Time.h"


void RigidTransformTest::Prepare()
{
    AddTest(
        "Default RigidTransform & Constants",
        [this](auto& ls)
        {
            RigidTR tm;
            ls << tm << lf;

            if (tm != tm)
            {
                ls << "Self equality failed" << lferr;
            }

            if (tm.rotation != Quat::Identity)
            {
                ls << "The default constructor of RigidTR should have "
                   << "the identity rotation." << lferr;
            }

            if (tm.translation != Float3::Zero)
            {
                ls << "The default constructor of RigidTR should have "
                   << "the zero translation." << lferr;
            }

            Float4x4 mat = tm.ToMatrix();
            if (mat != Float4x4::Identity)
            {
                ls << "The default constructor of RigidTR should be "
                   << "equal to the identity matrix." << lferr;
            }

            if (tm * Float3::Forward != Float3::Forward)
            {
                ls << "Identity Transform Failed. " << (tm * Float3::Forward) << ", but "
                   << Float3::Forward << " expected." << lferr;
            }
        });

    AddTest(
        "RigidTransform Operations",
        [this](auto& ls)
        {
            RigidTR tm(Float3(4, 5, 6), Quat(0, 47, 0));
            RigidTR tm2(Float3(3, 4, 5), Quat(41, 0, 25));
            RigidTR tm3 = tm * tm2;

            ls << tm << lf;
            ls << tm2 << lf;
            ls << tm3 << lf;

            Float3 result1 = tm3 * Float3::Forward;
            Float3 result2 = tm * (tm2 * Float3::Forward);
            if (result1 != result2)
            {
                ls << "Transform result failed. " << (tm3 * Float3::Forward) << ", but " << result2
                   << " expected." << lferr;
            }

            if (!tm3.ToMatrix().IsInvertible())
            {
                ls << "Transform matrix should be invertible." << tm3 << lferr;
            }

            Float3 result = tm3.Inverse() * result1;
            if (result != Float3::Forward)
            {
                ls << "Transform matrix inverse failed. " << result << ", but " << Float3::Forward
                   << " expected." << lferr;
            }

            result = tm2.InverseTransform(tm.InverseTransform(result2));
            if (result != Float3::Forward)
            {
                ls << "Inverse transform failed. " << result << ", but " << Float3::Forward
                   << " expected." << lferr;
            }

            if (tm3.Inverse().Inverse() != tm3)
            {
                ls << "Inverse of inverse transform failed. " << tm3.Inverse().Inverse() << ", but "
                   << tm3 << " expected." << lferr;
            }
        });
}

#endif //__UNIT_TEST__
