#include "UniformTransform.h"

#ifdef __UNIT_TEST__

#include "Matrix4x4.h"

#include "Container/Vector.h"
#include "System/Time.h"
#include <iostream>


using namespace HE;

bool UniformTransformTest::DoTest()
{
  using namespace std;

  {
    UniformTRS tm;
    cout << tm << endl;

    if (tm != tm)
    {
      cerr << "Self equality failed" << endl;

      return false;
    }

    if (tm.rotation != Quat::Identity)
    {
      cerr << "The default constructor of UniformTrs should have "
        << "the identity rotation." << endl;

      return false;
    }

    if (tm.scale != 1.0f)
    {
      cerr << "The default constructor of UniformTrs should have "
        << "the identity scale." << endl;

      return false;
    }

    if (tm.translation != Float3::Zero)
    {
      cerr << "The default constructor of UniformTrs should have "
        << "the zero translation." << endl;

      return false;
    }

    Float4x4 mat = tm.ToMatrix();
    if (mat != Float4x4::Identity)
    {
      cerr << "The default constructor of UniformTrs should be "
        << "equal to the identity matrix." << endl;
      cerr << mat << endl;

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
    UniformTRS tm(Float3(4, 5, 6), Quat(0, 47, 0), 2);
    UniformTRS tm2(Float3(3, 4, 5), Quat(41, 0, 25), 3);
    Float4x4 tm3 = (tm * tm2).ToMatrix();

    Float3 result1 = tm3 * Float4(Float3::Forward, 1.0f);
    Float3 result2 = tm * (tm2 * Float3::Forward);
    if (result1 != result2)
    {
      cerr << "Transform result failed. " << (tm3 * Float3::Forward)
        << ", but " << result2 << " expected." << endl;
      return false;
    }

    if (!tm3.IsInvertible())
    {
      cerr << "Transform matrix should be invertible." << endl
        << tm3 << endl;

      return false;
    }

    Float3 result = tm3.Inverse() * Float4(result1, 1.0f);
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
