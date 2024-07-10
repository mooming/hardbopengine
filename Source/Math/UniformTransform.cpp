#include "UniformTransform.h"

#ifdef __UNIT_TEST__
#include "Matrix4x4.h"
#include "System/Time.h"


void HE::UniformTransformTest::Prepare()
{
    AddTest(
        "Constants",
        [this](auto& ls)
        {
            UniformTRS tm;
            ls << tm << lf;

            if (tm != tm)
            {
                ls << "Self equality failed" << lferr;
            }

            if (tm.rotation != Quat::Identity)
            {
                ls << "The default constructor of UniformTrs should have "
                   << "the identity rotation." << lferr;
            }

            if (tm.scale != 1.0f)
            {
                ls << "The default constructor of UniformTrs should have "
                   << "the identity scale." << lferr;
            }

            if (tm.translation != Float3::Zero)
            {
                ls << "The default constructor of UniformTrs should have "
                   << "the zero translation." << lferr;
            }

            Float4x4 mat = tm.ToMatrix();
            if (mat != Float4x4::Identity)
            {
                ls << "The default constructor of UniformTrs should be "
                   << "equal to the identity matrix." << std::endl
                   << mat << lferr;
            }

            if (tm * Float3::Forward != Float3::Forward)
            {
                ls << "Identity Transform Failed. " << (tm * Float3::Forward) << ", but "
                   << Float3::Forward << " expected." << lferr;
            }
        });

    AddTest(
        "Inverse Matrix",
        [this](auto& ls)
        {
            UniformTRS tm(Float3(4, 5, 6), Quat(0, 47, 0), 2);
            UniformTRS tm2(Float3(3, 4, 5), Quat(41, 0, 25), 3);
            Float4x4 tm3 = (tm * tm2).ToMatrix();

            Float3 result1 = tm3 * Float4(Float3::Forward, 1.0f);
            Float3 result2 = tm * (tm2 * Float3::Forward);

            if (result1 != result2)
            {
                ls << "Transform result failed. " << (tm3 * Float3::Forward) << ", but " << result2
                   << " expected." << lferr;
            }

            if (!tm3.IsInvertible())
            {
                ls << "Transform matrix should be invertible." << std::endl << tm3 << lferr;
            }

            Float3 result = tm3.Inverse() * Float4(result1, 1.0f);
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
