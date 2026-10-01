// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.
// Created by Hansol Park (mooming.go@gmail.com), 2025

#pragma once
#include <algorithm>

#include <cstdint>
#include "Config/BuildConfig.h"

namespace hbe
{
/// @brief A bitmask template for specifying which task streams a task can execute on.
/// @details One bit per stream, densely packed: bit `i` lives in word `i / 64` at shift `i % 64`. The word count used
/// to
///          be derived from `BitArrayUnitBytes` - bytes per word where the division needs *bits* per word - which made
///          the buffer eight times as long as it needed and left each 64-bit word holding only 8 usable bits, because
///          the index helper divided by the same figure. Both halves agreed with each other, so the type was correct
///          and merely fat; a queued work item paid 64 bytes for a 64-stream mask. The size is now pinned by a test in
///          this module's collection, because no behavioural test can tell the two encodings apart - each is internally
///          consistent, which is exactly why the defect survived.
/// @note `Get` refuses indices at or above `NumBits`, so padding bits in a partially filled last word are unreachable
///       and their zero value (which this encoding reads as "set") never becomes permission to run something.
template <unsigned int NumBits>
class TaskStreamAffinityBase final
{
private:
	using TBitArrayUnit = uint64_t;
	static constexpr size_t BitArrayUnitBytes = sizeof(TBitArrayUnit);
	static constexpr size_t BitsPerUnit = BitArrayUnitBytes * 8;
	static constexpr size_t BitsArraySize = (NumBits + BitsPerUnit - 1) / BitsPerUnit;

	// Bit 0: set
	// Bit 1: unset
	TBitArrayUnit bitBuffer[BitsArraySize];

public:
	[[nodiscard]] static constexpr auto GetNumBits() noexcept
	{
		return NumBits;
	}

	TaskStreamAffinityBase()
		: bitBuffer{0}
	{
	}

	void Unset(unsigned int bitIndex) noexcept
	{
		if (bitIndex >= NumBits)
		{
			return;
		}

		const auto index = GetBitsArrayIndexOf(bitIndex);
		auto& value = bitBuffer[index];
		const auto numShift = bitIndex - (index * BitsPerUnit);
		TBitArrayUnit bit = 1;
		bit = bit << numShift;

		value = value | bit;
	}

	void Set(unsigned int bitIndex) noexcept
	{
		if (bitIndex >= NumBits)
		{
			return;
		}

		const auto index = GetBitsArrayIndexOf(bitIndex);
		auto& value = bitBuffer[index];
		const auto numShift = bitIndex - (index * BitsPerUnit);
		TBitArrayUnit mask = 1;
		mask = mask << numShift;
		mask = ~mask;

		value = value & mask;
	}

	[[nodiscard]] bool Get(unsigned int bitIndex) const noexcept
	{
		if (bitIndex >= NumBits)
		{
			return false;
		}

		const auto index = GetBitsArrayIndexOf(bitIndex);
		auto value = bitBuffer[index];
		const auto numShift = bitIndex - (index * BitsPerUnit);
		value = value >> numShift;

		// 0: Set, 1: Unset
		return (value & 1) == 0;
	}

private:
	[[nodiscard]] unsigned int GetBitsArrayIndexOf(unsigned int bitIndex) const noexcept
	{
		return bitIndex / BitsPerUnit;
	}
};

using TaskStreamAffinity = TaskStreamAffinityBase<64>;
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

class TaskStreamAffinityTest : public TestCollection
{
public:
	TaskStreamAffinityTest()
		: TestCollection("TaskStreamAffinityTest")
	{
	}

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
