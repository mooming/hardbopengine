// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "Quaternion.h"

using namespace std;
using namespace HE;

#ifdef __UNIT_TEST__

#include "Vector3.h"

#include <iostream>

bool QuaternionTest::DoTest()
{
  using namespace std;

  Quat x(90.0f, 0.0f, 0.0f);
  Quat y(0.0f, 90.0f, 0.0f);
  Quat z(0.0f, 0.0f, 90.0f);

  cout << "Quat rotate 90 around X = " << x << endl;
  cout << "Quat rotate 90 around Y = " << y << endl;
  cout << "Quat rotate 90 around Z = " << z << endl;

  cout << "Quat: Qx x Forward = " << x * Float3::Forward << endl;
  cout << "Quat: Qy x Forward = " << y * Float3::Forward << endl;
  cout << "Quat: Qz x Forward = " << z * Float3::Forward << endl;

#ifdef __LEFT_HANDED__
  if (y * Float3::Right != Float3::Forward)
  {
    cerr << "Quat rotation failed. " << (y * Float3::Right) << ", but "
      << Float3::Forward << " expected." << endl;
    return false;
  }
#endif //__LEFT_HANDED__
#ifdef __RIGHT_HANDED__
  if (z * Float3::Right != Float3::Forward)
  {
    cerr << "Quat rotation failed. " << (z * Float3::Right) << ", but "
      << Float3::Forward << " expected." << endl;
    return false;
  }
#endif //__RIGHT_HANDED__

  Quat xToY(nullptr);
  xToY.SetRotationFromTo(Float3::X, Float3::Y);

  if (xToY * Float3::X != Float3::Y)
  {
    cerr << "Quat from-to rotation failed. " << (xToY * Float3::X) << ", but "
      << Float3::Y << " expected." << endl;

    return false;
  }

  Quat yx(90.0f, 90.0f, 0.0f);
  cout << "Quat YX = " << yx << endl;
  cout << "Quat X x Y = " << (x * y) << endl;
  cout << "Quat Y x X = " << (y * x) << endl;

  if (yx != (y * x))
  {
    cerr << "Quat: yx is not equal to (y * x)." << endl;
    return false;
  }

  Quat zyx(90.0f, 90.0f, 90.0f);
  cout << "Quat ZYX = " << zyx << endl;
  cout << "Quat X x Y x Z = " << (x * y * z) << endl;
  cout << "Quat Z x Y x X = " << (z * y * x) << endl;

  if (zyx != (z * y * x))
  {
    cerr << "Quat: zyx is not equal to (z * y * x)." << endl;
    return false;
  }

  Float3x3 matZyx = zyx;

  if ((matZyx * Float3::Forward) != (zyx * Float3::Forward))
  {
    cerr << "Quat: matrix representation is not coincident." << endl;
    return false;
  }

  if (Quat() != Quat::CreateRotationX(0.0f))
  {
    cerr << "Quat: CreateRotationX(0) failed." << endl;
    return false;
  }

  if (Quat() != Quat::CreateRotationY(0.0f))
  {
    cerr << "Quat: CreateRotationY(0) failed." << endl;
    return false;
  }

  if (Quat() != Quat::CreateRotationXY(0.0f, 0.0f))
  {
    cerr << "Quat: CreateRotationXY(0) failed." << endl;
    return false;
  }

  if (Quat() != Quat::CreateRotationYZ(0.0f, 0.0f))
  {
    cerr << "Quat: CreateRotationYZ(0) failed." << endl;
    return false;
  }

  if (Quat() != Quat::CreateRotationXZ(0.0f, 0.0f))
  {
    cerr << "Quat: CreateRotationXZ(0) failed." << endl;
    return false;
  }

  if (Quat() != Quat(0.0f, 0.0f, 0.0f))
  {
    cerr << "Quat(0, 0, 0) failed." << endl;
    return false;
  }

  Quat look = Quat::LookRotation(Float3::Forward, Float3::Up);

  cout << "Look = " << look << endl;
  if (look != Quat())
  {
    cerr << "Quat: LookRotation failed." << endl;
    return false;
  }

  Quat lookBack = Quat::LookRotation(-Float3::Forward, Float3::Up);
  cout << "Look Backward = " << lookBack << endl;

  if ((lookBack * Float3::Forward) != (-Float3::Forward))
  {
    cerr << "Quat: LookRotation, look back failed." << endl;
    return false;
  }

  if ((y * Float3::Forward) != (Float3x3(y) * Float3::Forward))
  {
    cerr << "Quat: Matrix rotation is not coincided. "
      << (y * Float3::Forward) << " != "
      << (Float3x3(y) * Float3::Forward) << endl;

    return false;
  }

  return true;
}

#endif //__UNIT_TEST__
