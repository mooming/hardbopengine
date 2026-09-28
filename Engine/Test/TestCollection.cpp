// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Test/TestCollection.h"

#include <cstdint>
#include <exception>

#include "Log/Logger.h"
#include "Memory/MemoryManager.h"

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

void TestCollection::ReportNullTestCase(const std::string_view testName) noexcept
{
	auto log = Logger::Get(GetName());
	log.OutError([testName](auto& ls)
	{ ls << "Test " << testName << " was registered with no callable, so it will not run"; });
}

bool TestCollection::RunTestAt(const std::size_t testIndex)
{
	if (testIndex >= tests.size())
	{
		return false;
	}

	const auto& testlet = tests[testIndex];
	const auto testName = testlet.GetName();
	const auto errorCursorBefore = errorMessages.size();

	const auto indexLabel = static_cast<uint32_t>(testIndex);

	lf.testIndex = indexLabel;
	lf.testName = testName;
	lfwarn.testIndex = indexLabel;
	lfwarn.testName = testName;
	lferr.testIndex = indexLabel;
	lferr.testName = testName;

	auto log = Logger::Get(GetName());
	log.Out([indexLabel, testName](auto& ls) { ls << "# TC" << indexLabel << '.' << testName << " #"; });

	{
		TLogOut logStream;

		MultiPoolAllocator alloc(testName);
		AllocatorScope scope(alloc);

		const auto globalBytesBefore = MemoryManager::GetGlobalAllocationBytes();
		const auto globalRequestsBefore = MemoryManager::GetGlobalAllocationCount();
		const auto globalFreedBytesBefore = MemoryManager::GetGlobalFreeBytes();
		const auto globalFreedCountBefore = MemoryManager::GetGlobalFreeCount();

		testlet.Run(logStream);

		const auto globalBytesOverBody = MemoryManager::GetGlobalAllocationBytes() - globalBytesBefore;
		const auto globalRequestsOverBody = MemoryManager::GetGlobalAllocationCount() - globalRequestsBefore;
		const auto globalFreedBytesOverBody = MemoryManager::GetGlobalFreeBytes() - globalFreedBytesBefore;

		/*
		 * Retention is what a ceiling has to be about. A stress loop can request hundreds of megabytes while
		 * holding almost none of it, and a leak of the same size holds every byte without requesting more. The
		 * comparison is unsigned, so the clamp is not decoration: an unsized delete reports no size and releases
		 * nothing against the total, which can push freed above requested.
		 */
		const auto retainedBytesOverBody =
				globalBytesOverBody > globalFreedBytesOverBody ? globalBytesOverBody - globalFreedBytesOverBody : 0;

		const auto retainedCeiling = testlet.GetMaxRetainedGlobalBytes();

		maxRetainedGlobalBytes =
				retainedBytesOverBody > maxRetainedGlobalBytes ? retainedBytesOverBody : maxRetainedGlobalBytes;

		if (retainedBytesOverBody > retainedCeiling)
		{
			log.OutError([indexLabel, testName, retainedBytesOverBody, retainedCeiling](auto& ls)
			{
				ls << "# TC" << indexLabel << '.' << testName << " retained " << retainedBytesOverBody
				   << " bytes of the global heap at the end of its body, over the " << retainedCeiling
				   << " byte ceiling "
				   << (retainedCeiling == MaxRetainedGlobalBytes ? "a testlet may hold"
																 : "this testlet declared for itself")
				   << ". Free it, or raise the ceiling with a reason that survives review #\n";
			});

			/*
			 * Recorded as an error as well as logged, because whether this testlet passed is decided by the
			 * error count, and a guard that only prints is a comment with a number in it.
			 */
			errorMessages.push_back(std::string(testName) + " retained more global heap than the ceiling allows");
		}

		globalAllocationBytes += globalBytesOverBody;
		globalAllocationCount += globalRequestsOverBody;
		testletsWithGlobalAllocations += globalRequestsOverBody == 0 ? 0 : 1;

		alloc.PrintUsage();

		if (globalRequestsOverBody != 0)
		{
			log.Out([indexLabel, testName, globalRequestsOverBody, globalBytesOverBody, globalFreedCountBefore,
					 globalFreedBytesOverBody, retainedBytesOverBody, retainedCeiling](auto& ls)
			{
				ls << "# TC" << indexLabel << '.' << testName << " global heap " << globalRequestsOverBody
				   << " cumulative requests, " << globalBytesOverBody << " bytes requested, "
				   << (MemoryManager::GetGlobalFreeCount() - globalFreedCountBefore) << " releases, retained "
				   << retainedBytesOverBody << " bytes, freed " << globalFreedBytesOverBody
				   << " bytes outside the allocator scope";

				// A testlet that asked for its own budget says so on every line it produces, so the exception is
				// legible in a log excerpt and not only in the source that registered it.
				if (retainedCeiling != MaxRetainedGlobalBytes)
				{
					ls << ", against a declared ceiling of " << retainedCeiling;
				}

				ls << " #\n";
			});
		}
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

	return tests[testIndex].GetName();
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

std::size_t TestCollection::GetGlobalAllocationBytes() const noexcept
{
	return globalAllocationBytes;
}

std::uint64_t TestCollection::GetGlobalAllocationCount() const noexcept
{
	return globalAllocationCount;
}

std::size_t TestCollection::GetTestletCountWithGlobalAllocations() const noexcept
{
	return testletsWithGlobalAllocations;
}

std::size_t TestCollection::GetMaxRetainedGlobalBytes() const noexcept
{
	return maxRetainedGlobalBytes;
}

} // namespace hbe
