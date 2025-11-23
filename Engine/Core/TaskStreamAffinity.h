// Created by Hansol Park (mooming.go@gmail.com), 2025

#pragma once
#include <algorithm>

#include <cstdint>
#include "Config/BuildConfig.h"

namespace hbe
{
	template<unsigned int NumBits>
	class TaskStreamAffinityBase final
	{
	private:
		using TBitArrayUnit = uint64_t;
		static constexpr size_t BitArrayUnitBytes = sizeof(TBitArrayUnit);
		static constexpr size_t BitsArraySize = (NumBits + BitArrayUnitBytes - 1) / BitArrayUnitBytes;

		// Bit 0: set
		// Bit 1: unset
		TBitArrayUnit bitBuffer[BitsArraySize];

	public:
		TaskStreamAffinityBase() : bitBuffer{0}
		{
		}

		static constexpr auto GetNumBits() { return NumBits; }

		void Unset(unsigned int bitIndex)
		{
			if (bitIndex >= NumBits)
			{
				return;
			}

			const auto index = GetBitsArrayIndexOf(bitIndex);
			auto& value = bitBuffer[index];
			const auto numShift = bitIndex - (index * BitArrayUnitBytes);
			TBitArrayUnit bit = 1;
			bit = bit << numShift;

			value = value | bit;
		}

		void Set(unsigned int bitIndex)
		{
			if (bitIndex >= NumBits)
			{
				return;
			}

			const auto index = GetBitsArrayIndexOf(bitIndex);
			auto& value = bitBuffer[index];
			const auto numShift = bitIndex - (index * BitArrayUnitBytes);
			TBitArrayUnit mask = 1;
			mask = mask << numShift;
			mask = ~mask;

			value = value & mask;
		}

		[[nodiscard]] bool Get(unsigned int bitIndex) const
		{
			if (bitIndex >= NumBits)
			{
				return false;
			}

			const auto index = GetBitsArrayIndexOf(bitIndex);
			auto value = bitBuffer[index];
			const auto numShift = bitIndex - (index * BitArrayUnitBytes);
			value = value >> numShift;

			// 0: Set, 1: Unset
			return (value & 1) == 0;
		}

	private:
		[[nodiscard]] unsigned int GetBitsArrayIndexOf(unsigned int bitIndex) const
		{
			return bitIndex / BitsArraySize;
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
		TaskStreamAffinityTest() : TestCollection("TaskStreamAffinityTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__


