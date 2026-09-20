// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hbe
{

/// @brief One task's result: an 8-byte header over a 120-byte payload, embedded in the registry record.
/// @details This is the whole of what the engine knows about results. A task writes one packet, and a task with
///          more to say than one packet holds puts a handle to memory it owns into the payload and owns that
///          memory itself - which is why there is no per-stream result buffer, no capacity to admit against and
///          no delivery window measured in frames. The result belongs to the task and lives as long as the task
///          is tracked.
/// @details The producer fills the header and payload; whoever the packet is addressed to reads them through the
///          declaring task's identity, so a reader naming a task that has been released is refused by the
///          registry's generation check rather than reading a recycled record. Nothing in the engine copies a
///          packet, and nothing in the engine frees one.
/// @note The header is not padding. Its first two bytes are the only things a reader needs before it can
///       interpret the payload - what the result is, and who asked for it - and both are written by the producer,
///       which is why routing never needed the header to grow past eight bytes.
/// @note Not thread-safe, and it does not need to be: one task writes its packet while it runs, and the reader
///       arrives after that task has finished. A task writing while another reads the same packet is a bug in
///       whoever declared the result, not a condition this type negotiates.
class alignas(std::uint64_t) ResultPacket final
{
public:
	/// @brief Bytes in the header, fixed by decision rather than by measurement.
	static constexpr std::size_t HeaderBytes = 8;

	/// @brief Bytes available to the producer after the header.
	static constexpr std::size_t PayloadBytes = 120;

	/// @brief Bytes in the whole packet. Because the packet is embedded rather than pooled, this is also the
	///        growth it adds to a task - and therefore to every record in the identity table.
	static constexpr std::size_t SizeBytes = HeaderBytes + PayloadBytes;

	/// @brief Kind meaning "no result has been written to this packet".
	/// @details A record is reused without being zeroed, so this is the state a packet comes back in and the state
	///          a reader stops at. It is zero because a packet that was never written to is a packet of zeroes.
	static constexpr std::uint8_t KindNoResult = 0;

	/// @brief First kind value owned by the application; below this is the engine's range.
	/// @details The split exists so the engine cannot allocate an application's kind by accident. It is a
	///          boundary rather than a table: nothing dispatches on it, and whoever sets a kind decides which side
	///          of it they are on.
	static constexpr std::uint8_t FirstApplicationKind = 64;

	/// @brief Byte of the header holding the kind.
	static constexpr std::size_t KindByteIndex = 0;

	/// @brief Byte of the header holding the destination stream index.
	static constexpr std::size_t DestinationByteIndex = 1;

	/// @brief Destination meaning "this result is addressed to no stream".
	/// @details Two real uses, and they are the two cases R16 already allows: work that reports nothing to anyone,
	///          and a caller that reads its own result through its own identity instead of being handed it.
	static constexpr std::uint8_t NoDestinationStream = 0xFF;

	static_assert(SizeBytes == 128, "A result packet must be the 128 bytes the design fixed");
	static_assert(HeaderBytes > DestinationByteIndex, "The header must be wide enough to hold both decided fields");

	/// @brief Forget every byte of this packet, putting it back to "no result".
	/// @details Called when a record is issued, not when a task is finished with: a reused record must not be able
	///          to report the previous occupant's result.
	void Clear() noexcept
	{
		std::memset(header, 0, sizeof(header));
		std::memset(payload, 0, sizeof(payload));
		header[DestinationByteIndex] = NoDestinationStream;
	}

	/// @brief Whether a producer has written a result into this packet.
	[[nodiscard]] bool HasResult() const noexcept
	{
		return header[KindByteIndex] != KindNoResult;
	}

	/// @brief What this result is. The producer's choice; the engine does not interpret it.
	[[nodiscard]] std::uint8_t GetKind() const noexcept
	{
		return header[KindByteIndex];
	}

	/// @brief Declare what this result is, and so - by the kind not being KindNoResult - that there is one.
	/// @note Call this after writing the payload, not before: a reader that arrives between the two would see a
	///       kind claiming a payload that has not been filled yet.
	void SetKind(std::uint8_t newKind) noexcept
	{
		header[KindByteIndex] = newKind;
	}

	/// @brief Which stream the result is addressed to.
	[[nodiscard]] std::uint8_t GetDestinationStreamIndex() const noexcept
	{
		return header[DestinationByteIndex];
	}

	void SetDestinationStreamIndex(std::uint8_t streamIndex) noexcept
	{
		header[DestinationByteIndex] = streamIndex;
	}

	/// @brief Writable payload, PayloadBytes long.
	[[nodiscard]] std::uint8_t* GetPayload() noexcept
	{
		return payload;
	}

	/// @brief Readable payload, PayloadBytes long.
	[[nodiscard]] const std::uint8_t* GetPayload() const noexcept
	{
		return payload;
	}

private:
	/// @brief The header as bytes: kind, then destination, then whatever of the eight bytes is left over.
	/// @details Deliberately not three named fields with a spare array between them - a spare field nobody reads is
	///          a warning this project treats as an error, and the leftover bytes are not a field with a meaning
	///          yet. The two fields that do have meaning are byte-indexed below.
	std::uint8_t header[HeaderBytes] = {KindNoResult, NoDestinationStream};

	std::uint8_t payload[PayloadBytes] = {};
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

/// @brief Test collection for the embedded result packet: its price, and the state a new record starts in.
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
