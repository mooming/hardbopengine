// Copyright, All rights reserved by mooming.go@gmail.com

#include "./EuclidSpace.h"

namespace HE
{
  template class EuclidSpace<float>;
}

#ifdef __UNIT_TEST__
#include <iostream>

bool HE::EuclidSpaceTest::DoTest()
{
  using std::cout;
  using std::endl;

  FloatSpace root;
  cout << "Root = " << root << endl;

  FloatSpace child;
  child.SetPosition( {1.0f, 1.0f, 1.0f});
  child.SetRotation( {0.0f, 45.0f, 0.0f});
  child.SetScale( {2.0f, 1.0f, 1.0f});
  child.SetParent(&root);

  FloatSpace child2;
  child2.SetPosition( {1.0f, 1.0f, 1.0f});
  child2.SetScale( {1.0f, 2.0f, 1.0f});
  child2.SetParent(&child);

  cout << "Child = " << child << endl;
  cout << "Child2 = " << child2 << endl;

  root.SetPosition( {0.0f, 1.0f, 2.0f});
  root.SetRotation( {0.0f, 0.0f, 45.0f});
  cout << "Moved Root = " << root << endl;

  cout << "Child Moved = " << child << endl;
  cout << "Child2 Moved = " << child2 << endl;

  return true;
}

#endif  // __UNIT_TEST__
