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
#else
	// Every test body in the engine sits behind #ifdef __UNIT_TEST__, including the ones this
	// executable links from the library modules. Built without the macro there is nothing left
	// to run, and returning 0 from there reads exactly like a passing suite - which is how a
	// build gate came to report success over zero tests. So say what is missing, how to get it,
	// and leave a non-zero status behind.
	std::cerr << R"(EngineTest: built WITHOUT __UNIT_TEST__, so this binary contains no tests.
Nothing has been verified, and the exit status used to claim otherwise.

To build and run the suite:
	./build.sh Applications/EngineTest -dev -debug -release -test
	./build/Applications/EngineTest/Dev/EngineTest        (or Debug/ or Release/)

-test adds -D__UNIT_TEST__ to the entire build tree, and it has to be global: the test
bodies live in the library sources this executable links, not only in this file. Leaving
them out silently is how whole modules stop being tested without anything failing.

The standards gate does both halves, building and then running the suite:
	.pi/skills/hb-standards/scripts/check.sh --test
)" << std::endl;
	return 1;
#endif // __UNIT_TEST__

	return 0;
}
