// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <iostream>

#include "Engine/Engine.h"
#include "Memory/MemoryManager.h"
#include "Test/TestEnv.h"
#include "Test/UnitTestCollection.h"

int main(int argc, const char* argv[]) noexcept
{
#ifdef __UNIT_TEST__
	constexpr auto BaseStreamPassBudget = std::chrono::duration<double>(0.001);

	hbe::Engine hengine;
	hengine.Initialize(argc, argv);

	hengine.GetTaskSystem().GetStream(hbe::TaskSystem::GetBaseTaskStreamIndex()).ConfigureBudget(BaseStreamPassBudget);

	hbe::Test::RegisterSuite();
	hbe::Test::ScheduleSuiteOnBaseStream();

	hengine.Run();

	const hbe::TestEnv& testEnv = hbe::TestEnv::GetEnv();

	if (testEnv.GetExecutedTestletCount() != testEnv.GetTestletCount())
	{
		std::cerr << "EngineTest: only " << testEnv.GetExecutedTestletCount() << " of " << testEnv.GetTestletCount()
				  << " registered testlets ran, so the engine never drove the suite it was given" << std::endl;
		return 1;
	}

	const unsigned int failures = testEnv.GetFailureCount();

	std::cout << "EngineTest: global heap over testlet bodies " << testEnv.GetGlobalAllocationCount()
			  << " cumulative requests, " << testEnv.GetGlobalAllocationBytes() << " cumulative bytes requested, over "
			  << testEnv.GetTestletCountWithGlobalAllocations() << " of " << testEnv.GetExecutedTestletCount()
			  << " testlets; whole process " << hbe::MemoryManager::GetGlobalAllocationCount()
			  << " cumulative requests, " << hbe::MemoryManager::GetGlobalAllocationBytes()
			  << " cumulative bytes requested" << std::endl;

	if (failures > 0)
	{
		std::cerr << "EngineTest: " << failures << " test collection(s) FAILED" << std::endl;
		return 1;
	}

	std::cout << "EngineTest: all " << testEnv.GetPassCount() << " collections passed ("
			  << testEnv.GetExecutedTestletCount() << " testlets)" << std::endl;

#else
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
