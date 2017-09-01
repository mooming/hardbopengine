// Copyright, All rights reserved by Hansol Park (mooming.go@gmail.com)

#include "CommonUtil.h"

#include "Platform.h"

#ifdef __UNIT_TEST__
#include <iostream>

bool HE::CommonUtilTest::DoTest()
{
    using namespace std;

    {
      int testArray[] = { 0, 1, 2 };

      cout << "Size of test array is " << CountOf(testArray) << endl;

      if (CountOf(testArray) != 3)
      {
        cerr << "CountOf: invalid result " << CountOf(testArray) << endl;
        return false;
      }
    }

    {
      cout << "2^10 = " << Pow(2, 10) << endl;

      if (Pow(2, 10) != 1024)
      {
        cerr << "Pow(int, int) result failed = " << Pow(2, 10) << ", but 1024"
          << " expected." << endl;
        return false;
      }
    }

    return true;
}

#endif //__UNIT_TEST__
