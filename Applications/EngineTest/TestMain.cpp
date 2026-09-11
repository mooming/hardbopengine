// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <iostream>

#include "Engine/Engine.h"
#include "Test/TestEnv.h"
#include "Test/UnitTestCollection.h"

int main(int argc, const char* argv[]) noexcept
{
#ifdef __UNIT_TEST__
	hbe::Engine hengine;
	hengine.initialize(argc, argv);
	hbe::Test::runTests();
	hengine.run();

	// The suite runs as a task that shuts the engine down when it finishes, so the tallies
	// are only complete once run() returns. A failing suite has to leave a non-zero exit
	// status behind: until now it always returned 0, which made the run impossible to gate on.
	const hbe::TestEnv& testEnv = hbe::TestEnv::getEnv();
	const unsigned int failures = testEnv.getFailureCount();

	if (failures > 0)
	{
		std::cerr << "EngineTest: " << failures << " test collection(s) FAILED" << std::endl;
		return 1;
	}

	std::cout << "EngineTest: all " << testEnv.getPassCount() << " collections passed" << std::endl;
#else
	// Every test body in the engine sits behind #ifdef __UNIT_TEST__, including the ones this
	// executable links from the library modules. Built without the macro there is nothing left
	// to run, and returning 0 from there reads exactly like a passing suite - which is how a
	// build gate came to report success over zero tests. So say what is missing, how to get it,
	// and leave a non-zero status behind.
	// Written as adjacent literals, one per line, rather than a raw string: the guide is
	// space-indented so it survives any tab width, and the lint rule against space-indented
	// source lines reads the FILE - so the spaces belong inside the literals, where they are
	// output, not at the start of a source line, where they would be code.
	std::cerr << "EngineTest: built WITHOUT __UNIT_TEST__, so this binary contains no tests.\n"
				 "Nothing has been verified, and the exit status used to claim otherwise.\n"
				 "\n"
				 "To build and run the suite:\n"
				 "    ./build.sh Applications/EngineTest -dev -debug -release -test\n"
				 "    ./build/Applications/EngineTest/Dev/EngineTest    (or Debug/ or Release/)\n"
				 "\n"
				 "-test adds -D__UNIT_TEST__ to the entire build tree, and it has to be global: the\n"
				 "test bodies live in the library sources this executable links, not only in this\n"
				 "file. Leaving them out silently is how whole modules stop being tested without\n"
				 "anything failing.\n"
				 "\n"
				 "The standards gate does both halves, building and then running the suite:\n"
				 "    .pi/skills/hb-standards/scripts/check.sh --test\n";
	return 1;
#endif // __UNIT_TEST__

	return 0;
}
