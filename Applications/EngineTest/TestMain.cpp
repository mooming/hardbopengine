// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <iostream>

#include "Engine/Engine.h"
#include "Memory/MemoryManager.h"
#include "Test/TestEnv.h"
#include "Test/UnitTestCollection.h"


int main(int argc, const char* argv[]) noexcept
{
#ifdef TEST_ENABLED
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
		std::cerr << "EngineTest: only " << testEnv.GetExecutedTestletCount() << " of " << testEnv.GetTestletCount();
		std::cerr << " registered testlets ran, so the engine never drove the suite it was given" << std::endl;

		return 1;
	}

	const unsigned int failures = testEnv.GetFailureCount();

	std::cout << "EngineTest: global heap over testlet bodies " << testEnv.GetGlobalAllocationCount()
			  << " cumulative requests, " << testEnv.GetGlobalAllocationBytes() << " cumulative bytes requested, over "
			  << testEnv.GetTestletCountWithGlobalAllocations() << " of " << testEnv.GetExecutedTestletCount()
			  << " testlets; whole process " << hbe::MemoryManager::GetGlobalAllocationCount()
			  << " cumulative requests, " << hbe::MemoryManager::GetGlobalAllocationBytes()
			  << " cumulative bytes requested, of which OS-image " << hbe::MemoryManager::GetOSAllocationCount()
			  << " cumulative requests, " << hbe::MemoryManager::GetOSAllocationBytes() << " cumulative bytes requested"
			  << std::endl;

	if (failures > 0)
	{
		std::cerr << "EngineTest: " << failures << " test collection(s) FAILED" << std::endl;

		return 1;
	}

	std::cout << "EngineTest: all " << testEnv.GetPassCount() << " collections passed ("
			  << testEnv.GetExecutedTestletCount() << " testlets)" << std::endl;

#else
	std::cerr << "EngineTest: built WITHOUT TEST_ENABLED, so this binary contains no tests." << std::endl;
	std::cerr << "Nothing has been verified, and the exit status used to claim otherwise." << std::endl;
	std::cerr << std::endl;
	std::cerr << "To build and run the suite:" << std::endl;
	std::cerr << "    ./build.sh Applications/EngineTest -dev -debug -release -test" << std::endl;
	std::cerr << "    ./build/Applications/EngineTest/Dev/EngineTest    (or Debug/ or Release/)" << std::endl;
	std::cerr << std::endl;
	std::cerr << "-test adds -DTEST_ENABLED to the entire build tree, and it has to be global: the" << std::endl;
	std::cerr << "test bodies live in the library sources this executable links, not only in this" << std::endl;
	std::cerr << "file. Leaving them out silently is how whole modules stop being tested without" << std::endl;
	std::cerr << "anything failing." << std::endl;
	std::cerr << std::endl;
	std::cerr << "The standards gate does both halves, building and then running the suite:" << std::endl;
	std::cerr << "    .pi/skills/hb-standards/scripts/check.sh --test" << std::endl;

	return 1;
#endif // TEST_ENABLED

	return 0;
}
