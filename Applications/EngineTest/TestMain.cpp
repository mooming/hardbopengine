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

	// The base stream is throttled, and the reason it is worth the extra passes is a future one rather than a present
	// one. Measured on the defect this harness exists to catch - Engine::Run's while reduced to a single pass - the
	// shortfall check refuses it with or without this allowance, because each testlet only posts the next one after it
	// runs, so the queue is empty the moment an item is taken and a pass that quits early cannot reach far. The chain
	// is the load; the allowance is the insurance. CPUBudget's zero means unlimited and CanTakeWork reads it once per
	// pass, so if the driver is ever changed to re-scan the queue after every completed item, an unbounded pass would
	// drain the whole chain at once and make the guard vacuous again - silently, which is how this was missed for five
	// commits. A bounded allowance keeps that from being possible. Too small costs only extra passes, never
	// correctness, which is the harmless direction and the reason for 1ms rather than a guessed larger figure.
	hengine.GetTaskSystem()
			.GetStream(hbe::TaskSystem::GetBaseTaskStreamIndex())
			.ConfigureBudget(std::chrono::duration<double>(0.001));

	// Registered before Run is entered, so the total the run is expected to reach belongs to the harness rather than
	// being discovered by the suite - and a suite that never started cannot report a total matching the nothing it ran.
	hbe::Test::RegisterSuite();
	hbe::Test::ScheduleSuiteOnBaseStream();

	// Every testlet is a task on the base stream and each one posts the next, so this loop is the only thing that can
	// carry the suite to its end. The last testlet reports and requests shutdown from inside the run, which is why
	// Run() returns with the tallies already settled and no separate shutdown call belongs here.
	hengine.Run();

	const hbe::TestEnv& testEnv = hbe::TestEnv::GetEnv();

	// The guardrail proper: registered is known from before the run and executed is what the run achieved, so a loop
	// that stopped iterating leaves a gap that cannot be argued away. Measured against the defect this exists for -
	// Engine::Run's while reduced to a single pass - the suite reaches only a fraction of its testlets, and this is
	// the line that refuses it.
	if (testEnv.GetExecutedTestletCount() != testEnv.GetTestletCount())
	{
		std::cerr << "EngineTest: only " << testEnv.GetExecutedTestletCount() << " of " << testEnv.GetTestletCount()
				  << " registered testlets ran, so the engine never drove the suite it was given" << std::endl;
		return 1;
	}

	const unsigned int failures = testEnv.GetFailureCount();

	if (failures > 0)
	{
		std::cerr << "EngineTest: " << failures << " test collection(s) FAILED" << std::endl;
		return 1;
	}

	std::cout << "EngineTest: all " << testEnv.GetPassCount() << " collections passed ("
			  << testEnv.GetExecutedTestletCount() << " testlets)" << std::endl;

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
