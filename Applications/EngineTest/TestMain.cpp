// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <iostream>

#include "Engine/Engine.h"
#include "Test/TestEnv.h"
#include "Test/UnitTestCollection.h"


int main(int argc, const char* argv[]) noexcept
{
#ifdef __UNIT_TEST__
	hbe::Engine hengine;
	hengine.Initialize(argc, argv);
	hbe::Test::RunTests();
	hengine.Run();

	// The suite runs as a task that shuts the engine down when it finishes, so the tallies
	// are only complete once Run() returns. A failing suite has to leave a non-zero exit
	// status behind: until now it always returned 0, which made the run impossible to gate on.
	const hbe::TestEnv& testEnv = hbe::TestEnv::GetEnv();
	const unsigned int failures = testEnv.GetFailureCount();

	if (failures > 0)
	{
		std::cerr << "EngineTest: " << failures << " test collection(s) FAILED" << std::endl;
		return 1;
	}

	std::cout << "EngineTest: all " << testEnv.GetPassCount() << " collections passed" << std::endl;
#endif // __UNIT_TEST__

	return 0;
}
