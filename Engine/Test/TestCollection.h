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
	/*
	 * How much inline storage each testlet of this suite gets, chosen here because the collections are what hold
	 * the testlets. A collection whose tests genuinely need a larger capture changes the alias below, and the
	 * static_assert in Testlet's constructor is what tells it so at the registration site.
	 */
	/*
	 * The most global heap memory a single testlet may still be holding when its body ends.
	 *
	 * Decided from the measured distribution rather than picked: across the 320 testlets that allocate at all,
	 * median retention is 48 bytes, the 99th percentile is 3,072, and the largest single figure is 33,200, from
	 * a thread-safety testlet. 64 KiB therefore refuses nothing today while leaving roughly two times headroom
	 * over the worst current offender, which is the shape a ceiling wants: a regression becomes visible, and the
	 * honest way past it is changing the test rather than the number.
	 *
	 * Two limits are known and deliberate. It counts the global door only - a testlet allocating through its
	 * AllocatorScope draws on MultiPoolAllocator banks, which are not in this figure, and closing that would
	 * mean adding a usage accessor to another module for a door measured to carry zero traffic today. And a
	 * release through the unsized operator delete reports the block size the system heap chose, which can exceed
	 * what was asked for, so retention can read slightly low.
	 */
	static constexpr std::size_t MaxRetainedGlobalBytes = 64 * 1024;

	static constexpr size_t MaxClosureBytes = 48;
	using TTestlet = Testlet<MaxClosureBytes>;

	/// @brief Longest test name this suite can hold, reported from the type that owns the storage.
	static constexpr size_t MaxTestNameBytes = TTestlet::MaxNameBytes;

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

	/*
	 * Registers one testlet. The name is a char array reference rather than a pointer or a view so that a
	 * literal is the only thing that can arrive: it has static storage, so Testlet may hold a view of it, and
	 * it is null terminated, which the logger and the per-testlet allocator both need. A computed name fails
	 * to compile here rather than dangling later.
	 *
	 * The closure is a template parameter and not a type-erased callable precisely so that no allocation can
	 * happen on the way in - the closure reaches Testlet with its real type, and Testlet's static_assert is
	 * what decides whether it fits. Call sites keep writing plain lambdas; nothing about the syntax changes.
	 */
	template <size_t TNameLength, typename TClosure>
	void AddTest(const char (&testName)[TNameLength], TClosure&& testCase)
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

		tests.emplace_back(testName, std::forward<TClosure>(testCase));
	}

	/// @brief How many testlets this collection registered. Zero until PrepareTests() has run.
	[[nodiscard]] std::size_t GetTestCount() const noexcept
	{
		return tests.size();
	}

	/// @brief The name a testlet registered itself under, for the task name and the failure report.
	/// @details Owned by the testlet and null terminated by construction, so callers may print it freely.
	[[nodiscard]] const char* GetTestName(std::size_t testIndex) const noexcept;

	[[nodiscard]] const char* GetName() const noexcept;

	/// @brief Bytes the testlets of this collection asked the global allocation entry points for.
	/// @details Measured across each testlet body, so it excludes the fixture that prepares it. This is the
	/// number an AllocatorScope cannot see: a std::vector or std::string inside a test reaches the heap through
	/// operator new, which no scope redirects, so without this a testlet could allocate freely off the books.
	[[nodiscard]] std::size_t GetGlobalAllocationBytes() const noexcept;
	/// @brief Requests this collection's testlet bodies sent to the global allocation entry points.
	[[nodiscard]] std::uint64_t GetGlobalAllocationCount() const noexcept;
	/// @brief How many of this collection's testlets reached the global heap at all while their body ran.
	/// @details The suite wants this count rather than the byte total when it decides which testlets must justify
	/// an allocation, because one request is the fact that matters and its size is only context.
	[[nodiscard]] std::size_t GetTestletCountWithGlobalAllocations() const noexcept;

	/// @brief The largest amount of global heap one of this collection's testlets was still holding at its end.
	/// @details Bytes requested during a body minus bytes released during it, so a testlet that allocates and
	/// frees in a loop scores here the way it should: heavily, on requests, and not at all on retention. The
	/// figure can over-estimate, never under-estimate, because an unsized delete reports no size and therefore
	/// releases nothing against the total.
	[[nodiscard]] std::size_t GetMaxRetainedGlobalBytes() const noexcept;
	[[nodiscard]] const std::vector<std::string>& GetWarningMessages() const noexcept;
	[[nodiscard]] const std::vector<std::string>& GetErrorMessages() const noexcept;
	[[nodiscard]] bool IsDone() const noexcept;
	[[nodiscard]] bool IsSuccess() const noexcept;

protected:
	/*
	 * Reports a registration that handed over no callable. Kept out of the template above so that the header
	 * needs no logger, which matters because the logger's own headers are order-sensitive about the allocator
	 * they use; a test file should not have to include that allocator first to compile.
	 */
	void ReportNullTestCase(std::string_view testName) noexcept;

	std::vector<TTestlet> tests;
	std::vector<std::string> warningMessages;
	std::vector<std::string> errorMessages;

	std::size_t globalAllocationBytes{0};
	std::uint64_t globalAllocationCount{0};
	std::size_t testletsWithGlobalAllocations{0};
	std::size_t maxRetainedGlobalBytes{0};

	friend std::ostream& operator<<(std::ostream& os, const LogFlush& lf);

	virtual void Prepare() = 0;

private:
	std::string title;
	bool isDone;
	bool isSuccess;

	void Report() const;
};

} // namespace hbe
