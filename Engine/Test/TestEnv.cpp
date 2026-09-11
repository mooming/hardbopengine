// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TestEnv.h"

#include <sstream>

#include "Core/Debug.h"
#include "Core/Exception.h"
#include "Log/Logger.h"
#include "TestCollection.h"


namespace hbe
{

TestEnv& TestEnv::getEnv()
{
	static TestEnv instance;
	return instance;
}

void TestEnv::start()
{
	invalidTests.clear();
	failedTests.clear();

	for (auto& testCase : tests)
	{
		if (testCase == nullptr)
		{
			std::cerr << "Error: testCase is null" << std::endl;
		}
		Assert(testCase != nullptr);
		executeTest(*testCase);
	}

	report();
}

bool TestEnv::executeTest(TestCollection& testCollection)
{
	testCollection.start();

	if (!testCollection.IsDone())
	{
		invalidTests.push_back(testCollection.getName());
	}
	else
	{
		using namespace std;
		std::stringstream ss;

		++testedCount;

		if (testCollection.IsSuccess())
		{
			++passCount;

			auto& warnMessages = testCollection.getWarningMessages();
			for (auto& msg : warnMessages)
			{
				ss << '[' << testCollection.getName() << "] " << msg;
				warningMessages.push_back(ss.str());
				ss.str("");
			}
		}
		else
		{
			ss << testCollection.getName() << " : [FAIL]";
			failedTests.push_back(ss.str());
			ss.str("");

			auto& warnMessages = testCollection.getWarningMessages();
			for (auto& msg : warnMessages)
			{
				ss << '[' << testCollection.getName() << "]" << msg;
				warningMessages.push_back(ss.str());
				ss.str("");
			}

			auto& errMessages = testCollection.getErrorMessages();
			for (auto& msg : errMessages)
			{
				ss << '[' << testCollection.getName() << "]" << msg;
				errorMessages.push_back(ss.str());
				ss.str("");
			}
		}
	}

	return testCollection.IsSuccess();
}

void TestEnv::report()
{
	using namespace std;
	auto log = Logger::get("TestEnv");

	log.out([this](auto& ls)
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
		log.outError("= Invalid Tests =============================");

		for (auto& item : invalidTests)
		{
			log.outError([&item](auto& ls) { ls << item; });
		}

		log.outError("=============================================");
	}

	if (!failedTests.empty())
	{
		log.outError("= Failed Tests ==============================");

		int index = 1;
		for (auto& item : failedTests)
		{
			log.outError([index, &item](auto& ls) { ls << index << ": " << item; });

			++index;
		}

		log.outError("=============================================\n");
	}

	if (!errorMessages.empty())
	{
		log.outError("= Errors ====================================");

		for (auto& item : errorMessages)
		{
			log.outError([&item](auto& ls) { ls << item; });
		}

		log.outError("=============================================\n");
	}

	if (!warningMessages.empty())
	{
		log.outWarning("= Warnings ==================================");

		for (auto& item : warningMessages)
		{
			log.outWarning([&item](auto& ls) { ls << item; });
		}

		log.outWarning("=============================================\n");
	}

	auto& logger = Logger::get();
	logger.flush();
}

} // namespace hbe
