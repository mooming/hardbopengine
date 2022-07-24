// Copyright, all rights are reserved by Hansol Park, mooming.go@gmail.compl

#include "AABB.h"

namespace HE
{
  template class AABB<Float2>;
  template class AABB<Float3>;
}

#ifdef __UNIT_TEST__

#include <iostream>

bool HE::AABBTest::DoTest()
{
  using std::cout;
  using std::cerr;
  using std::endl;

  {
    AABB3 bbox;

    cout << bbox << endl;

    if (!bbox.IsEmpty())
    {
      cerr << "AABB created with default constructor should be empty." << endl;

      return false;
    }

    bbox.Add(Float3::Zero);
    bbox.Add(Float3::Forward);
    bbox.Add(Float3::Right);
    bbox.Add(Float3::Up);

    cout << "bbox = " << bbox << endl;

    if (bbox.Diagonal() != Float3::Unity)
    {
      cerr << "AABB dianoal error. " << bbox.Diagonal()
        << ", but expected " << Float3::Unity << endl;

      return false;
    }

    if (!bbox.IsContaining(bbox))
    {
      cerr << "AABB should contain itself! " << bbox << endl;

      return false;
    }

    if (bbox != bbox)
    {
      cerr << "AABB should match itself! " << bbox << endl;

      return false;
    }

    if (bbox.Intersection(bbox) != bbox)
    {
      cerr << "Self intersection of AABB should match itself! "
        << bbox << endl;

      return false;
    }

    auto bbox2 = bbox;
    if (bbox != bbox2)
    {
      cerr << "Clone of AABB should match itself! "
        << bbox << ", " << bbox2 << endl;

      return false;
    }

    bbox2.max *= 2.0f;
    cout << "bbox2 = " << bbox2 << endl;

    if (bbox2.Intersection(bbox) != bbox)
    {
      cerr << "Invalid Intersection of AABB. Intersection of "
        << bbox2 << " and " << bbox << " = "
        << bbox2.Intersection(bbox) << endl;

      return false;
    }

    if ((bbox + bbox2) != bbox2)
    {
      cerr << "Invalid union of AABB. Union of "
        << bbox << " and " << bbox2 << " = "
        << (bbox + bbox2) << endl;

      return false;
    }

    auto bbox3 = bbox2;
    bbox3.Translate(-Float3::Unity);

    cout << "bbox3 = " << bbox3 << endl;
    if (bbox2.Intersection(bbox3) != bbox)
    {
      cerr << "Invalid Intersection of AABB. Intersection of "
        << bbox2 << " and " << bbox3 << " = "
        << bbox2.Intersection(bbox3)
        << ", but expecting " << bbox << endl;

      return false;
    }

    bbox2.Translate(Float3::Unity);
    if (!bbox2.Intersection(bbox3).IsEmpty())
    {
      cerr << "Invalid Intersection of AABB. " << endl
        << "Intersection of " << endl
        << bbox2 << endl
        << " and " << endl
        << bbox3 << endl
        << "is " << bbox2.Intersection(bbox3) << endl
        << "But expecting the empty." << endl;

      return false;
    }
  }

  return true;
}

#endif  // __UNIT_TEST__