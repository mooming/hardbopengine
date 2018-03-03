// Copyright, All rights reserved by Hansol Park (mooming.go@gmail.com)

#include "MathUtil.h"

#include "Platform.h"

#ifdef __UNIT_TEST__
#include <iostream>

bool HE::MathUtilTest::DoTest()
{
    using namespace std;

    {
      cout << "2^10 = " << Pow(2, 10) << endl;

      if (Pow(2, 10) != 1024)
      {
        cerr << "Pow(int, int) result failed = " << Pow(2, 10) << ", but 1024"
          << " expected." << endl;

        return false;
      }

      cout << "2.0f^10.0f = " << Pow(2.0f, 10.0f) << endl;
      if (Pow(2, 10) != Pow(2.0f, 10.0f))
      {
        cerr << "Pow(float, float) result failed = " << Pow(2.0f, 10.0f)
          << ", but 1024.0f" << " expected." << endl;

        return false;
      }
    }

    return true;
}

#endif //__UNIT_TEST__
