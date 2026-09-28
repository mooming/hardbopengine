// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TestEnv.h"

#include <iostream>
#include <sstream>

#include "Core/Debug.h"
#include "Core/Exception.h"
#include "Log/Logger.h"
#include "TestCollection.h"

namespace hbe
{

TestEnv& TestEnv::GetEnv()
{
	static TestEnv instance;
	return instance;
}

void TestEnv::PrepareTestlets()
{
	collectionOffsets.clear();
	testletLabels.clear();
	executedTestletCount = 0;

	std::size_t runningTotal = 0;

	for (auto& test : tests)
	{
		Assert(test != nullptr);

		test->PrepareTests();

		for (std::size_t testIndex = 0; testIndex < test->GetTestCount(); ++testIndex)
		{
			std::stringstream label;
			label << test->GetName() << " TC" << testIndex << '.' << test->GetTestName(testIndex);
			testletLabels.push_back(label.str());
		}

		runningTotal += test->GetTestCount();
		collectionOffsets.push_back(runningTotal);
	}

	testletRunCounts.assign(testletLabels.size(), 0);
}

const char* TestEnv::GetTestletLabel(const std::size_t testletIndex) const
{
	if (testletIndex >= testletLabels.size())
	{
		return "";
	}

	return testletLabels[testletIndex].c_str();
}

void TestEnv::RunTestlet(const std::size_t testletIndex)
{
	if (testletIndex >= testletLabels.size())
	{
		std::cerr << "Error: testlet index " << testletIndex << " is past the " << testletLabels.size()
				  << " testlets the suite registered, so the run cannot be scheduled as far as it is reaching."
				  << std::endl;
		return;
	}

	std::size_t collectionIndex = 0;
	while (testletIndex >= collectionOffsets[collectionIndex])
	{
		++collectionIndex;
	}

	const std::size_t firstTestletOfCollection = collectionIndex == 0 ? 0 : collectionOffsets[collectionIndex - 1];
	auto& testCollection = *tests[collectionIndex];

	if (++testletRunCounts[testletIndex] > 1)
	{
		NoteTestletRepeated(testletIndex);
	}

	testCollection.RunTestAt(testletIndex - firstTestletOfCollection);
	++executedTestletCount;

	if (testletIndex + 1 == collectionOffsets[collectionIndex])
	{
		FinalizeCollection(testCollection);
	}
}

/// @brief Close one collection whose last testlet has just run, and tally it exactly as the batch loop used to.
/// @details The collection reports its own testlets; this decides what the collection as a whole contributes
///          to the suite: a pass, a failure, and the warnings it left behind.
void TestEnv::FinalizeCollection(TestCollection& testCollection)
{
	testCollection.Complete();

	if (!testCollection.IsDone())
	{
		invalidTests.push_back(testCollection.GetName());
	}
	else
	{
		using namespace std;
		std::stringstream ss;

		++testedCount;

		if (testCollection.IsSuccess())
		{
			++passCount;

			auto& warnMessages = testCollection.GetWarningMessages();
			for (auto& msg : warnMessages)
			{
				ss << '[' << testCollection.GetName() << "] " << msg;
				warningMessages.push_back(ss.str());
				ss.str("");
			}
		}
		else
		{
			ss << testCollection.GetName() << " : [FAIL]";
			failedTests.push_back(ss.str());
			ss.str("");

			auto& warnMessages = testCollection.GetWarningMessages();
			for (auto& msg : warnMessages)
			{
				ss << '[' << testCollection.GetName() << "]" << msg;
				warningMessages.push_back(ss.str());
				ss.str("");
			}

			auto& errMessages = testCollection.GetErrorMessages();
			for (auto& msg : errMessages)
			{
				ss << '[' << testCollection.GetName() << "]" << msg;
				errorMessages.push_back(ss.str());
				ss.str("");
			}
		}
	}
}

void TestEnv::Finalize()
{
	Report();
}

void TestEnv::Report()
{
	using namespace std;
	auto log = Logger::Get("TestEnv");

	log.Out([this](auto& ls)
	{
		ls << hendl;
		ls << "##### TEST COMPLETED #####" << hendl;
		ls << "# Total Count = " << tests.size() << hendl;
		ls << "# Test Done = " << testedCount << hendl;
		ls << "# Invalid Test = " << invalidTests.size() << hendl;
		ls << "# Pass = " << passCount << hendl;
		ls << "# Fail = " << failedTests.size() << hendl;
		ls << "##### TEST Report Done #####" << hendl;
	});

	if (!invalidTests.empty())
	{
		log.OutError("= Invalid Tests =============================");

		for (auto& item : invalidTests)
		{
			log.OutError([&item](auto& ls) { ls << item; });
		}

		log.OutError("=============================================");
	}

	if (!failedTests.empty())
	{
		log.OutError("= Failed Tests ==============================");

		int index = 1;
		for (auto& item : failedTests)
		{
			log.OutError([index, &item](auto& ls) { ls << index << ": " << item; });

			++index;
		}

		log.OutError("=============================================\n");
	}

	if (!errorMessages.empty())
	{
		log.OutError("= Errors ====================================");

		for (auto& item : errorMessages)
		{
			log.OutError([&item](auto& ls) { ls << item; });
		}

		log.OutError("=============================================\n");
	}

	if (!warningMessages.empty())
	{
		log.OutWarning("= Warnings ==================================");

		for (auto& item : warningMessages)
		{
			log.OutWarning([&item](auto& ls) { ls << item; });
		}

		log.OutWarning("=============================================\n");
	}

	auto& logger = Logger::Get();
	logger.Flush();
}

} // namespace hbe
