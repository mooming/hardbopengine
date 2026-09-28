// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstring>
#include <iostream>
#include <new>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Core/Debug.h"

/*
 * One test: the name it registered under, and the lambda that runs it, held inline.
 *
 * This type exists to make registering a test unable to allocate. The alternative was std::function, whose
 * buffer is an implementation detail: a closure larger than that buffer takes a heap allocation silently, and
 * the suite has no way to see it, because a global operator new bypasses every allocator scope the tests are
 * built to measure. Deciding the storage here instead turns that silent case into a compile error at the
 * registration site, and the ceiling is a number the suite owns rather than one inherited from a library.
 *
 * The name is a view for the same reason, and the cost of getting it wrong was measurable: 151 of the 378 test
 * names in this suite are longer than a small-string buffer holds, so a std::string member meant a heap
 * allocation at registration for two in five tests, on the way into the very suite that audits allocations. A
 * view is honest only while the referred text outlives the testlet, which literals do; TestCollection::AddTest
 * accepts nothing but a char array, so a computed name fails to compile at the call site instead of dangling
 * later, and its null termination - which the per-testlet allocator's name needs - is a property of the type
 * rather than of a comment.
 *
 * Why not a non-owning view of the callable, which would also allocate nothing: the suite registers closures it
 * never keeps. Every AddTest hands over a temporary lambda, and a view cannot extend that lifetime, so it would
 * dangle the moment the call returned - or refuse to compile, which is the same outcome with better manners.
 * Owning the bytes inline is what makes the closure's lifetime exactly the testlet's.
 *
 * Moving a testlet relocates the closure rather than copying it, which is why the dispatch table carries a
 * relocate hook and why a moved-from testlet reports no table. Copies are deleted: a copy would need a clone
 * hook for an arbitrary closure type, and nothing in the suite asks to duplicate a test. Because this is a
 * class template, every member is defined here, which is what the coding standard permits for templates.
 *
 * Errors here go to std::cerr rather than the engine logger, following the rest of the Test module, and
 * deliberately so: pulling Log/Logger.h into this header makes every test file in the engine depend on the
 * logger's own include order, which is not something a test should have to get right to compile.
 */

namespace hbe
{
/* POD dispatch table: one constexpr instance per closure type, so the storage below needs no vptr and no allocation. */
struct TestletDispatch
{
	void (*invoke)(const void* object, std::stringstream& log);
	void (*destroy)(void* object) noexcept;
	void (*relocate)(void* source, void* destination) noexcept;
};

/// @brief One registered test, owning its closure inline.
/// @tparam TClosureBytes Inline storage reserved for the closure. It is a parameter rather than a constant
///           fixed inside the type because the ceiling has to be set where the captures are known - by whoever
///           holds the testlets - and because raising it is then a visible edit at the use site instead of a
///           change to a shared type. Measured basis for the default: 375 of the 379 closures in this suite fit
///           in 32 bytes and the four that do not are all 40 (the importance-sampling tests, whose [&] captures
///           hold pointers to locals, not the generator they share, which is static). 48 is that maximum plus
///           one word, the smallest allowance that leaves no testlet looking out at a fallback.
template <size_t TClosureBytes = 48>
class Testlet final
{
public:
	using TLogOut = std::stringstream;

	/// @brief The storage this instantiation reserved, reported back to callers that chose it.
	static constexpr size_t ClosureBytes = TClosureBytes;

	/*
	 * Storage for the test's name: long enough for the longest name measured in the suite today (105 characters)
	 * with room for one more clause. Enforced rather than hoped for, so a longer name is a compile error at the
	 * site that registered it.
	 */
	static constexpr size_t MaxNameBytes = 128;

	static_assert(TClosureBytes > 0, "A testlet with no inline storage can only ever hold a null callable");
	static_assert(TClosureBytes % sizeof(void*) == 0, "TClosureBytes should be a whole number of words");
	static_assert(MaxNameBytes > 1, "MaxNameBytes has to leave room for a name and its terminator");

	/*
	 * Constructs the closure in place. noexcept is deliberately absent: constructing an arbitrary closure is the
	 * case the coding standard points at when it says not to promise exception freedom for something whose
	 * implementation this type cannot see.
	 */
	template <size_t TNameBytes, typename TClosure>
		requires(!std::same_as<std::remove_cvref_t<TClosure>, Testlet>)
	Testlet(const char (&testName)[TNameBytes], TClosure&& closure)
		: dispatch(&GetDispatch<std::remove_cvref_t<TClosure>>())
	{
		static_assert(TNameBytes <= MaxNameBytes,
					  "A test's name does not fit Testlet::MaxNameBytes. The buffer is inline so that naming a test "
					  "cannot allocate; shorten the label to what the test actually tests, or raise MaxNameBytes where "
					  "the reason for a longer label is stated - the one thing not available is borrowing somebody "
					  "else's storage, or quietly truncating a label a failure report depends on.");
		static_assert(TNameBytes > 1, "A test must have a name worth reporting");

		std::memcpy(name, testName, TNameBytes);
		static_assert(
				sizeof(std::remove_cvref_t<TClosure>) <= TClosureBytes,
				"A testlet's captured state does not fit the TClosureBytes it was given, which is inline storage "
				"that exists so registering a test cannot allocate. Shrink the capture, or instantiate Testlet with "
				"more bytes where the testlets are held and say why - the one thing not available is falling back "
				"to the heap quietly.");
		static_assert(
				alignof(std::remove_cvref_t<TClosure>) <= alignof(std::max_align_t),
				"A testlet's closure is over-aligned for this inline buffer; over-aligned captures need their own "
				"storage rather than quietly reducing the testlet to an unaligned placement.");

		new (static_cast<void*>(storage)) std::remove_cvref_t<TClosure>(std::forward<TClosure>(closure));
	}

	Testlet(Testlet&& other) noexcept
		: dispatch(other.dispatch)
	{
		std::memcpy(name, other.name, MaxNameBytes);
		if (other.dispatch != nullptr)
		{
			other.dispatch->relocate(other.storage, storage);
			other.dispatch = nullptr;
		}
	}

	Testlet(const Testlet&) = delete;
	Testlet& operator=(const Testlet&) = delete;
	Testlet& operator=(Testlet&&) = delete;

	~Testlet()
	{
		if (dispatch != nullptr)
		{
			dispatch->destroy(storage);
		}
	}

	/*
	 * Runs the closure. outLog is a write-only parameter by the standard's out-prefix convention: the testlet's
	 * whole effect is what it writes there, plus whatever its assertions record.
	 *
	 * noexcept is a promise about the suite, not about this function: a testlet signals failure by logging and
	 * asserting, and the engine is exception-free, so nothing here is expected to unwind. A testlet that ever
	 * throws would terminate rather than unwind the run, which is the loud version of the alternative.
	 *
	 * A testlet that has been moved from reports itself and does nothing, rather than running a closure that no
	 * longer lives in its storage.
	 */
	void Run(TLogOut& outLog) const noexcept
	{
		if (dispatch == nullptr)
		{
			std::cerr << "Error: testlet " << name << " has no dispatch table, so it was moved from and cannot run"
					  << std::endl;

			Assert(dispatch != nullptr);

			return;
		}

		dispatch->invoke(storage, outLog);
	}

	/// @brief The registered name, viewing storage that outlives the run because AddTest accepts literals only.
	[[nodiscard]] const char* GetName() const noexcept
	{
		return name;
	}

private:
	template <typename TClosure>
	static void InvokeClosure(const void* object, TLogOut& outLog)
	{
		const TClosure& closure = *static_cast<const TClosure*>(object);

		closure(outLog);
	}

	template <typename TClosure>
	static void DestroyClosure(void* object) noexcept
	{
		static_cast<TClosure*>(object)->~TClosure();
	}

	template <typename TClosure>
	static void RelocateClosure(void* source, void* destination) noexcept
	{
		TClosure& movingFrom = *static_cast<TClosure*>(source);

		new (destination) TClosure(std::move(movingFrom));
		movingFrom.~TClosure();
	}

	/* A constexpr object per closure type, so its address is stable and nothing is allocated to hold it. */
	template <typename TClosure>
	static constexpr const TestletDispatch& GetDispatch() noexcept
	{
		static constexpr TestletDispatch dispatch{&InvokeClosure<TClosure>, &DestroyClosure<TClosure>,
												  &RelocateClosure<TClosure>};

		return dispatch;
	}

	char name[MaxNameBytes];
	const TestletDispatch* dispatch;
	alignas(alignof(std::max_align_t)) std::byte storage[TClosureBytes];
};

} // namespace hbe
