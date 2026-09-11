// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Test/TestCollection.h"

#include <exception>

#include "Log/Logger.h"


namespace hbe
{

TestCollection::LogFlush::LogFlush(const char* name, ELogLevel level, TLogBuffer* buffer) :
	name(name), level(level), testIndex(0), testName("None"), messageBuffer(buffer)
{}

TestCollection::TestCollection(const char* inTitle) :
	lf(inTitle, ELogLevel::Info, nullptr),
	lfwarn(inTitle, ELogLevel::Warning, &warningMessages),
	lferr(inTitle, ELogLevel::Error, &errorMessages),
	title(inTitle), isDone(false), isSuccess(false)
{}

const char* TestCollection::getName() const noexcept
{
	return title.c_str();
}

const std::vector<std::string>& TestCollection::getWarningMessages() const noexcept
{
	return warningMessages;
}

const std::vector<std::string>& TestCollection::getErrorMessages() const noexcept
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

void TestCollection::start()
{
	isDone = false;
	isSuccess = false;
	tests.clear();
	warningMessages.clear();
	errorMessages.clear();

	auto log = Logger::get(getName());

	log.out("= START ========================================");

	prepare();
	executeTests();

	isSuccess = errorMessages.empty();
	isDone = true;

	report();
}

void TestCollection::addTest(const char* name, const TTestFunc& testCase)
{
	if (unlikely(testCase == nullptr))
	{
		auto log = Logger::get(getName());
		log.outError([](auto& ls) { ls << "Null test-case error."; });

		return;
	}

	tests.emplace_back(name != nullptr ? name : "None", testCase);
}

void TestCollection::executeTests()
{
	TLogOut logStream;

	size_t errorCursor = 0;
	const size_t length = tests.size();

	for (uint32_t i = 0; i < length; ++i)
	{
		auto& testPair = tests[i];
		auto testName = testPair.first.c_str();

		lf.testIndex = i;
		lf.testName = testName;
		lfwarn.testIndex = i;
		lfwarn.testName = testName;
		lferr.testIndex = i;
		lferr.testName = testName;

		auto& test = testPair.second;
		if (test == nullptr)
		{
			std::cerr << "Error: test is null" << std::endl;
		}
		Assert(test != nullptr);

		auto log = Logger::get(getName());
		log.out([i, testName](auto& ls) { ls << "# TC" << i << '.' << testName << " #"; });

		{
			MultiPoolAllocator alloc(testName);
			AllocatorScope scope(alloc);
			test(logStream);

			alloc.printUsage();
		}

		auto newErrorCursor = errorMessages.size();
		bool isPassed = newErrorCursor == errorCursor;
		errorCursor = newErrorCursor;

		log.out([i, isPassed, testName](auto& ls)
		{
			ls << "# TC" << i << '.' << testName << " Result ";
			if (isPassed)
			{
				ls << "[PASS] #\n";
			}
			else
			{
				ls << "[FAIL] #\n";
			}
		});

		logStream.str("");
	}
}

void TestCollection::report() const
{
	TLog log(getName(), ELogLevel::Info);

	if (isSuccess)
	{
		log.out([](auto& ls) { ls << "= Collection Result: [SUCCESS] =================\n"; });
	}
	else
	{
		log.outError([](auto& ls) { ls << "= Collection Result: [FAIL] ====================\n"; });
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
	auto log = Logger::get(lf.name, lf.level);

	log.out([&lf, &prefix, &str](auto& ls)
	{
		ls << '[' << prefix.c_str() << "." << lf.testName << "] " << str.c_str();

		auto messages = lf.messageBuffer;
		if (messages == nullptr) return;

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

	auto log = Logger::get(lf.name, lf.level);
	log.out([&lf, &os](auto& ls)
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
