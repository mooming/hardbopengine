// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "Log/LogLevel.h"
#include "Testlet.h"


namespace hbe
{
template <class Predicate>
bool WaitUntil(Predicate&& isReady, const std::uint32_t timeoutMilliSecs = 2000) noexcept
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliSecs);

	while (!isReady() && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return isReady();
}

/// API reference: docs/Test/TestCollection/index.html
class TestCollection
{
public:
	using TLogOut = std::stringstream;
	using TLogBuffer = std::vector<std::string>;

	class LogFlush final
	{
	public:
		const char* name;
		ELogLevel level;
		uint32_t testIndex;
		const char* testName;
		TLogBuffer* messageBuffer;

		LogFlush(const char* name, ELogLevel level, TLogBuffer* buffer);
		~LogFlush() = default;
	};

	static constexpr size_t MaxClosureBytes = 48;
	using TTestlet = Testlet<MaxClosureBytes>; // hb-standards:ignore
	static constexpr std::size_t MaxRetainedGlobalBytes = 64 * 1024;

	static constexpr size_t MaxTestNameBytes = TTestlet::MaxNameBytes;
	LogFlush lf;
	LogFlush lfwarn;
	LogFlush lferr;

protected:
	std::vector<TTestlet> tests;
	std::vector<std::string> warningMessages;
	std::vector<std::string> errorMessages;
	std::size_t globalAllocationBytes{0};
	std::uint64_t globalAllocationCount{0};
	std::size_t testletsWithGlobalAllocations{0};
	std::size_t maxRetainedGlobalBytes{0};

private:
	std::string title;
	bool isDone;
	bool isSuccess;

public:
	explicit TestCollection(const char* title);
	virtual ~TestCollection() = default;

	void PrepareTests();

	bool RunTestAt(std::size_t testIndex);

	void Complete();

	template <size_t TNameLength, typename TClosure>
	void AddTest(const char (&testName)[TNameLength], TClosure&& testCase)
	{
		AddTest(testName, MaxRetainedGlobalBytes, std::forward<TClosure>(testCase));
	}

	template <size_t TNameLength, typename TClosure>
	void AddTest(const char (&testName)[TNameLength], std::size_t maxRetainedGlobalBytes, TClosure&& testCase)
	{
		using TClosureType = std::remove_cvref_t<TClosure>;
		if constexpr (std::is_pointer_v<TClosureType>)
		{
			if (testCase == nullptr)
			{
				ReportNullTestCase(testName);

				return;
			}
		}

		tests.emplace_back(testName, maxRetainedGlobalBytes, std::forward<TClosure>(testCase));
	}

	[[nodiscard]] std::size_t GetTestCount() const noexcept
	{
		return tests.size();
	}

	[[nodiscard]] const char* GetTestName(std::size_t testIndex) const noexcept;
	[[nodiscard]] const char* GetName() const noexcept;
	[[nodiscard]] std::size_t GetGlobalAllocationBytes() const noexcept;
	[[nodiscard]] std::uint64_t GetGlobalAllocationCount() const noexcept;

	[[nodiscard]] std::size_t GetTestletCountWithGlobalAllocations() const noexcept;

	[[nodiscard]] std::size_t GetMaxRetainedGlobalBytes() const noexcept;
	[[nodiscard]] const std::vector<std::string>& GetWarningMessages() const noexcept;
	[[nodiscard]] const std::vector<std::string>& GetErrorMessages() const noexcept;
	[[nodiscard]] bool IsDone() const noexcept;
	[[nodiscard]] bool IsSuccess() const noexcept;

protected:
	void ReportNullTestCase(std::string_view testName) noexcept;

	virtual void Prepare() = 0;

private:
	void Report() const;
	friend std::ostream& operator<<(std::ostream& os, const LogFlush& lf);
};
} // namespace hbe
