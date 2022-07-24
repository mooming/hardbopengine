// Copyright, all rights are reserved by Hansol Park, mooming.go@gmail.compl

#include "OBB.h"

namespace HE
{
	template class OBB<float>;
}

#ifdef __UNIT_TEST__

#include <iostream>

bool HE::OBBTest::DoTest()
{
	return true;
}

#endif  // __UNIT_TEST__
