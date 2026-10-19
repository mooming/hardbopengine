// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <iostream>

#include "Engine/Engine.h"
#include "Test/TestEnv.h"
#include "Test/UnitTestCollection.h"


int main(int argc, const char* argv[]) noexcept
{

#ifdef TEST_ENABLED
	constexpr auto BaseStreamPassBudget = std::chrono::duration<double>(0.001);

	hbe::Engine hengine;
	hengine.Initialize(argc, argv);

	hengine.GetTaskSystem().GetStream(hbe::TaskSystem::GetBaseTaskStreamIndex()).ConfigureBudget(BaseStreamPassBudget);

	hbe::Test::RegisterPerfBenchmarkSuite();
	hbe::Test::ScheduleSuiteOnBaseStream();

	hengine.Run();

	const hbe::TestEnv& testEnv = hbe::TestEnv::GetEnv();

	if (testEnv.GetExecutedTestletCount() != testEnv.GetTestletCount())
	{
		std::cerr << "RingBufferBenchmark: only " << testEnv.GetExecutedTestletCount() << " of "
				  << testEnv.GetTestletCount() << " sweep cells ran, so the sweep never reached its registered end"
				  << std::endl;

		return 1;
	}

	std::cout << "RingBufferBenchmark: " << testEnv.GetExecutedTestletCount()
			  << " sweep cells reported; a cell is information and never a pass or a failure" << std::endl;

	return 0;
#else
	std::cerr << "RingBufferBenchmark: built WITHOUT TEST_ENABLED, so this binary contains no performance cells."
			  << std::endl;
	std::cerr << "Rebuild with: ./build.sh RingBufferBenchmark -release -test" << std::endl;

	return 1;
#endif
}
