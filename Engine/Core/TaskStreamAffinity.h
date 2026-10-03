// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include <cstdint>

#include "Config/BuildConfig.h"


namespace hbe
{
/// API reference: docs/Core/TaskStreamAffinityBase/index.html
template <unsigned int NumBits>
class TaskStreamAffinityBase final
{
private:
	using TBitArrayUnit = uint64_t;
	static constexpr size_t BitArrayUnitBytes = sizeof(TBitArrayUnit);
	static constexpr size_t BitsPerUnit = BitArrayUnitBytes * 8;
	static constexpr size_t BitsArraySize = (NumBits + BitsPerUnit - 1) / BitsPerUnit;

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
