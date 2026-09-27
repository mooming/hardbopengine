// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "TestCollection.h"

namespace hbe
{

class TestEnv final
{
public:
	TestEnv() noexcept
		: testedCount(0)
		, passCount(0)
	{
	}

	[[nodiscard]] static TestEnv& GetEnv();
	void Start();

	/// @brief Collections that completed successfully.
	[[nodiscard]] unsigned int GetPassCount() const noexcept
	{
		return passCount;
	}

	/// @brief Collections that ran and failed, plus any that never completed.
	/// @details Valid after Start(); Start() clears both lists on entry.
	[[nodiscard]] unsigned int GetFailureCount() const noexcept
	{
		return static_cast<unsigned int>(failedTests.size() + invalidTests.size()) +
			   (suiteDrivenByShutdownPump ? 1u : 0u);
	}

	/// @brief Record that the suite began only because the shutdown path pumped it, and refuse to call that a pass.
	/// @details Reported as a failure rather than printed, because a suite that prints its own defect and still exits
	///          0 is exactly the shape that let a missing engine loop ship green once already.
	/// @note Held as its own flag, not appended to `failedTests`: `Start()` clears that list on entry, and the whole
	///       point is to record this before the suite has started. The first version of this recorded into a buffer
	///       that was wiped two statements later, which is why it passed over the defect it existed to catch.
	void NoteSuiteDrivenByShutdownPump() noexcept
	{
		suiteDrivenByShutdownPump = true;
	}

	template <typename T, typename... Types>
	void AddTestCollection(Types&&... args)
	{
		tests.push_back(std::make_unique<T>(std::forward(args)...));
	}

private:
	using TCPtr = std::unique_ptr<TestCollection>;

	std::vector<TCPtr> tests;
	std::vector<std::string> invalidTests;
	std::vector<std::string> failedTests;
	std::vector<std::string> warningMessages;
	std::vector<std::string> errorMessages;

	unsigned int testedCount;
	unsigned int passCount;

	bool suiteDrivenByShutdownPump{false};

	bool ExecuteTest(TestCollection& testCollection);
	void Report();
};

} // namespace hbe
