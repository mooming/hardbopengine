// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Test/TestCollection.h"

#include <exception>

#include "Log/Logger.h"

namespace hbe
{

TestCollection::LogFlush::LogFlush(const char* name, ELogLevel level, TLogBuffer* buffer)
	: name(name)
	, level(level)
	, testIndex(0)
	, testName("None")
	, messageBuffer(buffer)
{
}

TestCollection::TestCollection(const char* inTitle)
	: lf(inTitle, ELogLevel::Info, nullptr)
	, lfwarn(inTitle, ELogLevel::Warning, &warningMessages)
	, lferr(inTitle, ELogLevel::Error, &errorMessages)
	, title(inTitle)
	, isDone(false)
	, isSuccess(false)
{
}

const char* TestCollection::GetName() const noexcept
{
	return title.c_str();
}

const std::vector<std::string>& TestCollection::GetWarningMessages() const noexcept
{
	return warningMessages;
}

const std::vector<std::string>& TestCollection::GetErrorMessages() const noexcept
{
	return errorMessages;
}

bool TestCollection::IsDone() const noexcept
{
	return isDone;
}

bool TestCollection::IsSuccess() const noexcept
{
	return isSuccess;
}

void TestCollection::PrepareTests()
{
	isDone = false;
	isSuccess = false;
	tests.clear();
	warningMessages.clear();
	errorMessages.clear();

	auto log = Logger::Get(GetName());

	log.Out("= START ========================================");

	Prepare();
}

void TestCollection::AddTest(const char* name, const TTestFunc& testCase)
{
	if (unlikely(testCase == nullptr))
	{
		auto log = Logger::Get(GetName());
		log.OutError([](auto& ls) { ls << "Null test-case error."; });

		return;
	}

	tests.emplace_back(name != nullptr ? name : "None", testCase);
}

bool TestCollection::RunTestAt(const std::size_t testIndex)
{
	if (testIndex >= tests.size())
	{
		return false;
	}

	const auto& testPair = tests[testIndex];
	const auto testName = testPair.first.c_str();
	const auto errorCursorBefore = errorMessages.size();

	const auto indexLabel = static_cast<uint32_t>(testIndex);

	lf.testIndex = indexLabel;
	lf.testName = testName;
	lfwarn.testIndex = indexLabel;
	lfwarn.testName = testName;
	lferr.testIndex = indexLabel;
	lferr.testName = testName;

	auto& test = testPair.second;
	if (test == nullptr)
	{
		std::cerr << "Error: test is null" << std::endl;
	}
	Assert(test != nullptr);

	auto log = Logger::Get(GetName());
	log.Out([indexLabel, testName](auto& ls) { ls << "# TC" << indexLabel << '.' << testName << " #"; });

	{
		TLogOut logStream;

		MultiPoolAllocator alloc(testName);
		AllocatorScope scope(alloc);
		test(logStream);

		alloc.PrintUsage();
	}

	const bool isPassed = errorMessages.size() == errorCursorBefore;

	log.Out([indexLabel, isPassed, testName](auto& ls)
	{
		ls << "# TC" << indexLabel << '.' << testName << " Result ";
		if (isPassed)
		{
			ls << "[PASS] #\n";
		}
		else
		{
			ls << "[FAIL] #\n";
		}
	});

	return isPassed;
}

const char* TestCollection::GetTestName(const std::size_t testIndex) const noexcept
{
	if (testIndex >= tests.size())
	{
		return "";
	}

	return tests[testIndex].first.c_str();
}

void TestCollection::Complete()
{
	isSuccess = errorMessages.empty();
	isDone = true;

	Report();
}

void TestCollection::Report() const
{
	TLog log(GetName(), ELogLevel::Info);

	if (isSuccess)
	{
		log.Out([](auto& ls) { ls << "= Collection Result: [SUCCESS] =================\n"; });
	}
	else
	{
		log.OutError([](auto& ls) { ls << "= Collection Result: [FAIL] ====================\n"; });
	}
}

std::ostream& operator<<(std::ostream& os, const TestCollection::LogFlush& lf)
{
	std::stringstream ss;
	ss << os.rdbuf();

	std::string prefix;
	prefix.reserve(32);
	prefix.append("TC");
	prefix.append(std::to_string(lf.testIndex));

	auto str = ss.str();
	auto log = Logger::Get(lf.name, lf.level);

	log.Out([&lf, &prefix, &str](auto& ls)
	{
		ls << '[' << prefix.c_str() << "." << lf.testName << "] " << str.c_str();

		auto messages = lf.messageBuffer;
		if (messages == nullptr)
			return;

		{
			std::stringstream msg;
			msg << ls.c_str();
			messages->push_back(msg.str());
		}
	});

	ss.str("");

	return os;

#if 0
	// Add prefix for the current test case.

	auto log = Logger::Get(lf.name, lf.level);
	log.Out([&lf, &os](auto& ls)
	{
		ls << "[TC" << lf.testIndex << "." << lf.testName << "] ";
		std::istreambuf_iterator<char> strIter(os.rdbuf()), endIter;
		while (strIter != endIter)
		{
			ls << *strIter;
			++strIter;
		}
	});

	return os;
#endif
}

} // namespace hbe
