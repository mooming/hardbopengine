// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>


namespace hbe
{
/// API reference: docs/Core/ResultPacket/index.html
class alignas(std::uint64_t) ResultPacket final
{
public:
	static constexpr std::size_t HeaderBytes = 8;
	static constexpr std::size_t PayloadBytes = 120;
	static constexpr std::size_t SizeBytes = HeaderBytes + PayloadBytes;

	static constexpr std::uint8_t KindNoResult = 0;
	static constexpr std::uint8_t FirstApplicationKind = 64;

	static constexpr std::size_t KindByteIndex = 0;
	static constexpr std::size_t DestinationByteIndex = 1;
	static constexpr std::uint8_t NoDestinationStream = 0xFF;

	static_assert(SizeBytes == 128, "A result packet must be the 128 bytes the design fixed");
	static_assert(HeaderBytes > DestinationByteIndex, "The header must be wide enough to hold both decided fields");

private:
	std::uint8_t header[HeaderBytes] = {KindNoResult, NoDestinationStream};

	std::uint8_t payload[PayloadBytes] = {};

public:
	void Clear() noexcept
	{
		std::memset(header, 0, sizeof(header));
		std::memset(payload, 0, sizeof(payload));
		header[DestinationByteIndex] = NoDestinationStream;
	}

	[[nodiscard]] bool HasResult() const noexcept
	{
		return header[KindByteIndex] != KindNoResult;
	}

	[[nodiscard]] std::uint8_t GetKind() const noexcept
	{
		return header[KindByteIndex];
	}

	void SetKind(std::uint8_t newKind) noexcept
	{
		header[KindByteIndex] = newKind;
	}

	[[nodiscard]] std::uint8_t GetDestinationStreamIndex() const noexcept
	{
		return header[DestinationByteIndex];
	}

	void SetDestinationStreamIndex(std::uint8_t streamIndex) noexcept
	{
		header[DestinationByteIndex] = streamIndex;
	}

	[[nodiscard]] std::uint8_t* GetPayload() noexcept
	{
		return payload;
	}

	[[nodiscard]] const std::uint8_t* GetPayload() const noexcept
	{
		return payload;
	}
};
} // namespace hbe

static_assert(sizeof(hbe::ResultPacket) == hbe::ResultPacket::SizeBytes,
			  "A result packet must be exactly its declared width, with no padding added by the compiler");

static_assert(alignof(hbe::ResultPacket) == sizeof(std::uint64_t),
			  "A packet payload must be reachable in aligned words, wherever the packet lands");

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
class ResultPacketTest final : public TestCollection
{
public:
	ResultPacketTest()
		: TestCollection("ResultPacketTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif //__UNIT_TEST__
