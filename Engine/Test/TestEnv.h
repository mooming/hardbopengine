// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "TestCollection.h"

namespace hbe
{
/// @brief Owns the test collections, their registration order, and the tally of what actually ran.
/// @details A testlet - one test lambda inside one collection - is the unit this can schedule individually, and
///          the unit the harness therefore holds the engine loop to: `Test::ScheduleSuiteOnBaseStream` posts one
///          work item per testlet, so the run has to be driven testlet after testlet. The count it expects is the
///          count registered *before* the run was posted, which is what stops a suite that never started from
///          certifying itself: an expected total that the suite discovers for itself would read zero, agree with
///          the zero that ran, and call the dead run balanced.
class TestEnv final
{
public:
	TestEnv() noexcept
		: testedCount(0)
		, passCount(0)
	{
	}

	[[nodiscard]] static TestEnv& GetEnv();

	template <typename T, typename... Types>
	void AddTestCollection(Types&&... args)
	{
		tests.push_back(std::make_unique<T>(std::forward(args)...));
	}

	/// @brief Prepare every registered collection's testlets and flatten them into one schedulable sequence.
	/// @details Call after the last AddTestCollection and before anything is posted: the flattened sequence is what
	///          the harness counts against, and a collection that was prepared after posting began would make that
	///          count a moving target.
	void PrepareTestlets();

	/// @brief How many testlets the whole suite registered - the total a run is expected to reach.
	[[nodiscard]] std::size_t GetTestletCount() const noexcept
	{
		return testletLabels.size();
	}

	/// @brief How many testlets have actually been run. Compare with GetTestletCount(); a shortfall is a failure.
	[[nodiscard]] std::size_t GetExecutedTestletCount() const noexcept
	{
		return executedTestletCount;
	}

	/// @brief "Collection TC<n>.<name>" for one testlet, used as its task name so the registry names what is stuck.
	[[nodiscard]] const char* GetTestletLabel(std::size_t testletIndex) const;

	/// @brief Run one testlet, and finalise its collection once its last testlet has run.
	/// @details Repeats are reported through NoteTestletRepeated and the testlet still runs, because a suite that
	///          refuses to execute what it was handed would hide the schedule defect that produced the repeat.
	/// @param testletIndex Index into the flattened sequence, not into a collection.
	/// @details Collections are finalised in registration order because their testlets are contiguous in that
	///          sequence and a collection's verdict needs all of them.
	void RunTestlet(std::size_t testletIndex);

	/// @brief Print the suite report. Call once, after the last testlet.
	void Finalize();

	/// @brief Bytes every testlet body asked the global allocation entry points for.
	/// @details Summed across collections, and counted over testlet bodies only - the fixture that prepares a
	/// testlet, and everything the process allocated before the first testlet ran, are outside it. A non-zero
	/// figure means testlets reach the heap through operator new, which AllocatorScope cannot redirect.
	[[nodiscard]] std::size_t GetGlobalAllocationBytes() const noexcept;
	/// @brief Requests every testlet body sent to the global allocation entry points.
	[[nodiscard]] std::uint64_t GetGlobalAllocationCount() const noexcept;
	/// @brief How many testlets reached the global heap at all while their body ran.
	[[nodiscard]] std::size_t GetTestletCountWithGlobalAllocations() const noexcept;

	/// @brief Collections that completed successfully.
	[[nodiscard]] unsigned int GetPassCount() const noexcept
	{
		return passCount;
	}

	/// @brief Collections registered, whether or not they ran.
	[[nodiscard]] unsigned int GetCollectionCount() const noexcept
	{
		return static_cast<unsigned int>(tests.size());
	}

	/// @brief Collections that ran and failed, plus any that never completed, plus the provenance finding below.
	/// @details Valid after every testlet has run and its collection been finalised.
	[[nodiscard]] unsigned int GetFailureCount() const noexcept
	{
		return static_cast<unsigned int>(failedTests.size() + invalidTests.size()) +
			   (suiteDrivenByShutdownPump ? 1u : 0u);
	}

	/// @brief Record that one testlet ran more than once, which no scheduling of this suite is allowed to do.
	/// @details The suite's own totals cannot see this defect: a chain whose every step is told it is step zero runs
	///          one testlet N times and still reports N of N executed, which is exactly how a real bug in this
	///          harness once looked green-right-up-to-the-registry-ceiling. Counting runs per testlet is what turns
	///          that into a named failure.
	/// @note The first offender is kept, and later ones append, so a report names the repeat rather than a count.
	void NoteTestletRepeated(std::size_t testletIndex)
	{
		failedTests.push_back("testlet index " + std::to_string(testletIndex) + " ran more than once");
	}

	/// @brief Record that a testlet ran while the shutdown path was pumping the base stream, not the engine loop.
	/// @details Reported as a failure rather than printed, because a suite that prints its own defect and still
	///          exits 0 is exactly the shape that let a missing engine loop ship green once already.
	/// @note Held as its own flag, not appended to `failedTests`: that list is cleared when the suite prepares, and
	///       a finding about who drove the run has to survive being recorded before the run is over.
	void NoteSuiteDrivenByShutdownPump() noexcept
	{
		suiteDrivenByShutdownPump = true;
	}

private:
	using TCPtr = std::unique_ptr<TestCollection>;

	std::vector<TCPtr> tests;
	std::vector<std::size_t> collectionOffsets;
	std::vector<std::string> testletLabels;
	std::vector<std::string> invalidTests;
	std::vector<std::string> failedTests;
	std::vector<std::string> warningMessages;
	std::vector<std::string> errorMessages;

	std::size_t executedTestletCount{0};
	std::vector<unsigned int> testletRunCounts;

	unsigned int testedCount;
	unsigned int passCount;

	bool suiteDrivenByShutdownPump{false};

	void FinalizeCollection(TestCollection& testCollection);
	void Report();
};

} // namespace hbe
