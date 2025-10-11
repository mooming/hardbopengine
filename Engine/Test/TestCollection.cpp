// Created by mooming.go@gmail.com, 2017 ~ 2022

#include "Test/TestCollection.h"

#include <exception>
#include "Log/Logger.h"

namespace hbe
{

	TestCollection::LogFlush::LogFlush(const char* name, ELogLevel level, TLogBuffer* buffer) :
		name(name), level(level), testIndex(0), testName("None")

		,
		messageBuffer(buffer)

	{}

	TestCollection::TestCollection(const char* inTitle) :
		title(inTitle), isDone(false), isSuccess(false), lf(title.c_str(), ELogLevel::Info, nullptr),
		lfwarn(title.c_str(), ELogLevel::Warning, &warningMessages),
		lferr(title.c_str(), ELogLevel::Error, &errorMessages)
	{}

	void TestCollection::Start()
	{
		isDone = false;
		isSuccess = false;
		tests.clear();
		warningMessages.clear();
		errorMessages.clear();

		auto log = Logger::Get(GetName());

		try
		{
			log.Out("= START ========================================");

			Prepare();
			ExecuteTests();

			isSuccess = errorMessages.empty();
			isDone = true;
		}
		catch (std::exception& e)
		{
			log.OutError([title = GetName(), &e](auto& ls) { ls << title << " : " << e.what() << hendl; });

			isSuccess = false;
		}

		Report();
	}

	void TestCollection::AddTest(const char* name, const TestFunc& testCase)
	{
		if (unlikely(testCase == nullptr))
		{
			auto log = Logger::Get(GetName());
			log.OutError([](auto& ls) { ls << "Null test-case error."; });

			return;
		}

		tests.emplace_back(name != nullptr ? name : "None", testCase);
	}

	void TestCollection::ExecuteTests()
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
			Assert(test != nullptr);

			auto log = Logger::Get(GetName());
			log.Out([i, testName](auto& ls) { ls << "# TC" << i << '.' << testName << " #"; });

			{
				MultiPoolAllocator alloc(testName);
				AllocatorScope scope(alloc);
				test(logStream);

				alloc.PrintUsage();
			}

			auto newErrorCursor = errorMessages.size();
			bool isPassed = newErrorCursor == errorCursor;
			errorCursor = newErrorCursor;

			log.Out([i, isPassed, testName](auto& ls)
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
			{
				return;
			}

			{
				std::stringstream msg;
				msg << ls.c_str();
				messages->push_back(msg.str());
			}
		});

		ss.str("");

		return os;
	}

} // namespace hbe
