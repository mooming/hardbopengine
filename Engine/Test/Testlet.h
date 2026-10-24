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


namespace hbe
{
/// API reference: docs/Test/TestletDispatch/index.html
struct TestletDispatch
{
	void (*invoke)(const void* object, std::stringstream& log);
	void (*destroy)(void* object) noexcept;
	void (*relocate)(void* source, void* destination) noexcept;
};

template <size_t TClosureBytes = 48>
/// API reference: docs/Test/Testlet/index.html
class Testlet final
{
public:
	using TLogOut = std::stringstream;
	static constexpr size_t ClosureBytes = TClosureBytes;

	static constexpr size_t MaxNameBytes = 128;

	static_assert(TClosureBytes > 0, "A testlet with no inline storage can only ever hold a null callable");
	static_assert(TClosureBytes % sizeof(void*) == 0, "TClosureBytes should be a whole number of words");
	static_assert(MaxNameBytes > 1, "MaxNameBytes has to leave room for a name and its terminator");

private:
	char name[MaxNameBytes];
	const TestletDispatch* dispatch;
	std::size_t maxRetainedGlobalBytes;
	alignas(alignof(std::max_align_t)) std::byte storage[TClosureBytes];

	template <size_t TNameBytes, typename TClosure>
		requires(!std::same_as<std::remove_cvref_t<TClosure>, Testlet>)
	Testlet(const char (&testName)[TNameBytes], std::size_t retainedByteCeiling, TClosure&& closure)
		: dispatch(&GetDispatch<std::remove_cvref_t<TClosure>>())
		, maxRetainedGlobalBytes(retainedByteCeiling)
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
		, maxRetainedGlobalBytes(other.maxRetainedGlobalBytes)
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

	[[nodiscard]] const char* GetName() const noexcept
	{
		return name;
	}

	[[nodiscard]] std::size_t GetMaxRetainedGlobalBytes() const noexcept
	{
		return maxRetainedGlobalBytes;
	}

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

	template <typename TClosure>
	static constexpr const TestletDispatch& GetDispatch() noexcept
	{
		static constexpr TestletDispatch dispatch{&InvokeClosure<TClosure>, &DestroyClosure<TClosure>,
												  &RelocateClosure<TClosure>};

		return dispatch;
	}
};
} // namespace hbe
