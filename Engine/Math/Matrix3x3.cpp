// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "Matrix3x3.h"

namespace HE
{
  template class Matrix3x3<float>;
}

#ifdef __UNIT_TEST__

#include "System/Vector.h"
#include <iostream>

bool HE::Matrix3x3Test::DoTest()
{
  using namespace std;

  const auto& right = Float3::Right;
  const auto& up = Float3::Up;
  const auto& forward = Float3::Forward;

#ifdef __LEFT_HANDED__
  Float3x3 rRight = Float3x3::CreateRotation(90, 0, 0);
  Float3x3 rUp = Float3x3::CreateRotation(0, 90, 0);
  Float3x3 rForward = Float3x3::CreateRotation(0, 0, 90);
#endif //__LEFT_HANDED__

#ifdef __RIGHT_HANDED__
  Float3x3 rRight = Float3x3::CreateRotation(90, 0, 0);
  Float3x3 rUp = Float3x3::CreateRotation(0, 0, 90);
  Float3x3 rForward = Float3x3::CreateRotation(0, 90, 0);
#endif //__RIGHT_HANDED__

  if (rRight * right != right)
  {
    cerr << "Rotation around right failed. "
      << (rRight * right) << ", but " << right << " expected." << endl;
    return false;
  }

  if (rUp * up != up)
  {
    cerr << "Rotation around Y failed. "
      << (rUp * up) << ", but " << up << " expected." << endl;
    return false;
  }

  if (rForward * forward != forward)
  {
    cerr << "Rotation around Z failed. "
      << (rForward * forward) << ", but " << forward << " expected." << endl;
    return false;
  }

  if (rRight * up != - forward)
  {
    cerr << "Rotation around X failed. "
      << (rRight * up) << ", but " << forward << " expected." << endl;
    return false;
  }

  if (rUp * forward != - right)
  {
    cerr << "Rotation around Y failed. "
      << (rUp * forward) << ", but " << right << " expected." << endl;
    return false;
  }

  if (rForward * right != - up)
  {
    cerr << "Rotation around Z failed. "
      << (rForward * right) << ", but " << up << " expected." << endl;
    return false;
  }

  return true;
}

#endif //__UNIT_TEST__
