// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Log/LogLevel.h"

namespace hbe
{
/// @brief Wait for a condition with a deadline, which is the only honest way a test can wait for work it dispatched.
/// @details A test that spins until a task's own counter says done cannot tell finished from never-dispatched, and
/// cannot
///          notice that the executor it depends on has gone; it hangs instead of failing. Every test wait therefore
///          carries a deadline and returns whether the condition was reached, so a stall becomes a reported failure.
/// @tparam Predicate Callable returning true once the thing being waited for has happened.
/// @param isReady Polled every millisecond until it says true or the timeout runs out.
/// @param timeoutMilliSecs How long the test is willing to wait before calling the condition unmet.
/// @return True if the condition was reached, false if the deadline passed first.
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

class TestCollection
{
public:
	using TLogOut = std::stringstream;
	using TLogBuffer = std::vector<std::string>;
	using TTestFunc = std::function<void(TLogOut& /*ls*/)>;

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

	LogFlush lf;
	LogFlush lfwarn;
	LogFlush lferr;

	explicit TestCollection(const char* title);
	virtual ~TestCollection() = default;

	/// @brief Register this collection's testlets without running any of them.
	/// @details The harness calls this for every collection before it posts anything, so the number of testlets in
	///          the whole suite is known outside the run rather than discovered by it. Prepare() only records
	///          lambdas, which is what makes it safe to run ahead of execution.
	void PrepareTests();

	/// @brief Run one testlet by index, with the same logging and per-testlet allocator scope the old batch loop gave
	/// it.
	/// @param testIndex Index into this collection's registered testlets, which PrepareTests() made enumerable.
	/// @return True if the testlet added no error message. A testlet past the registered count returns false.
	bool RunTestAt(std::size_t testIndex);

	/// @brief Close the collection: decide success from the errors its testlets accumulated, then report.
	void Complete();

	void AddTest(const char* testName, const TTestFunc& testCase);

	/// @brief How many testlets this collection registered. Zero until PrepareTests() has run.
	[[nodiscard]] std::size_t GetTestCount() const noexcept
	{
		return tests.size();
	}

	/// @brief The name a testlet registered itself under, for the task name and the failure report.
	[[nodiscard]] const char* GetTestName(std::size_t testIndex) const noexcept;

	[[nodiscard]] const char* GetName() const noexcept;
	[[nodiscard]] const std::vector<std::string>& GetWarningMessages() const noexcept;
	[[nodiscard]] const std::vector<std::string>& GetErrorMessages() const noexcept;
	[[nodiscard]] bool IsDone() const noexcept;
	[[nodiscard]] bool IsSuccess() const noexcept;

protected:
	std::vector<std::pair<std::string, TTestFunc>> tests;
	std::vector<std::string> warningMessages;
	std::vector<std::string> errorMessages;

	friend std::ostream& operator<<(std::ostream& os, const LogFlush& lf);

	virtual void Prepare() = 0;

private:
	std::string title;
	bool isDone;
	bool isSuccess;

	void Report() const;
};

} // namespace hbe
