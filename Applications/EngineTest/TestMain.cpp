// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Engine/Engine.h"
#include "Test/UnitTestCollection.h"


int main(int argc, const char* argv[]) noexcept
{
#ifdef __UNIT_TEST__
	hbe::Engine hengine;
	hengine.Initialize(argc, argv);
    hbe::Test::RunTests();
	hengine.Run();
#endif // __UNIT_TEST__

	return 0;
}
