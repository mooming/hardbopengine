// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "ResultContainer.h"

#include "Core/Debug.h"
#include "Memory/MultiPoolAllocator.h"

namespace hbe
{

ResultContainer::ResultContainer(MultiPoolAllocator& ownerAllocator, std::size_t initialCapacitySlots) noexcept
	: slots(NamedPoolAllocator<TResultSlot>(ownerAllocator), static_cast<TSlots::TIndex>(initialCapacitySlots))
{
}

TResultSlot* ResultContainer::Append() noexcept
{
	Assert(count < GetCapacity(), "Result container is full: slot", count, "of", GetCapacity(),
		   "was claimed, so room was not secured before the work that fills it was taken on.");

	return &slots[static_cast<TSlots::TIndex>(count++)];
}

void ResultContainer::Grow(std::size_t slotsToAdd) noexcept
{
	const auto grownCapacity = GetCapacity() + slotsToAdd;
	slots.Resize(static_cast<TSlots::TIndex>(grownCapacity));

	Assert(GetCapacity() == grownCapacity, "Could not grow a result container to", grownCapacity, "slots; it holds",
		   GetCapacity(), ", which is the only room the next task can be admitted against.");
}

} // namespace hbe

#ifdef __UNIT_TEST__
namespace hbe
{

namespace
{

constexpr std::size_t TestCapacitySlots = 8;
constexpr std::size_t TestFilledSlots = 3;

void FillSlotWithPattern(TResultSlot& slot, std::size_t slotIndex)
{
	for (std::size_t byte = 0; byte < TResultSlot::SizeBytes; ++byte)
	{
		slot.bytes[byte] = static_cast<std::uint8_t>(slotIndex * 7U + byte + 3U);
	}
}

bool SlotHoldsPattern(const TResultSlot& slot, std::size_t slotIndex)
{
	for (std::size_t byte = 0; byte < TResultSlot::SizeBytes; ++byte)
	{
		if (slot.bytes[byte] != static_cast<std::uint8_t>(slotIndex * 7U + byte + 3U))
		{
			return false;
		}
	}

	return true;
}

} // namespace

void ResultContainerTest::Prepare()
{
	static_assert(ResultContainer::SlotSizeBytes == 128, "A result slot is 128 bytes by decision, not by accident.");
	static_assert(sizeof(TResultSlot) == 128, "The slot type must stay exactly as wide as the decided slot.");

	AddTest("A freshly built container holds the capacity it was given", [this](auto& ls)
	{
		MultiPoolAllocator pool("ResultContainerTest");
		ResultContainer container(pool, TestCapacitySlots);

		if (container.GetCapacity() != TestCapacitySlots)
		{
			ls << "A container built with " << TestCapacitySlots << " slots reports a capacity of "
			   << container.GetCapacity() << '.' << lferr;
		}

		if (container.GetCount() != 0)
		{
			ls << "A freshly built container reports " << container.GetCount() << " slots already claimed." << lferr;
		}

		if (container.GetEpoch() != 0)
		{
			ls << "A freshly built container starts at epoch " << container.GetEpoch() << ", not 0." << lferr;
		}

		const auto first = container.Append();
		const auto firstAddress = reinterpret_cast<std::uintptr_t>(first);
		if (firstAddress % 8 != 0)
		{
			ls << "The first result slot sits at an address with remainder " << (firstAddress % 8) << " mod 8, so it"
			   << " is not 8-byte aligned and a result could not keep a pointer inside it." << lferr;
		}

		const auto second = container.Append();
		const auto stride = reinterpret_cast<std::uintptr_t>(second) - firstAddress;
		ls << "Slot stride measured = " << stride << " bytes, count after two appends = " << container.GetCount() << '.'
		   << lf;

		if (stride != ResultContainer::SlotSizeBytes)
		{
			ls << "Consecutive slots are " << stride << " bytes apart; slots are fixed at "
			   << ResultContainer::SlotSizeBytes << " bytes, which is what makes an index a shift." << lferr;
		}

		if (container.GetCount() != 2)
		{
			ls << "Two appends left the count at " << container.GetCount() << '.' << lferr;
		}
	});

	AddTest("A container built with no capacity holds nothing", [this](auto& ls)
	{
		MultiPoolAllocator pool("ResultContainerTest");
		ResultContainer container(pool, 0);

		ls << "No-capacity container: capacity = " << container.GetCapacity() << ", count = " << container.GetCount()
		   << ", epoch = " << container.GetEpoch() << '.' << lf;

		if (container.GetCapacity() != 0)
		{
			ls << "A container built with no capacity reports " << container.GetCapacity() << " slots, so it took"
			   << " memory from a stream that never asked for any." << lferr;
		}

		if (container.GetCount() != 0 || container.GetEpoch() != 0)
		{
			ls << "A container built with no capacity starts out used: count = " << container.GetCount()
			   << ", epoch = " << container.GetEpoch() << '.' << lferr;
		}
	});

	AddTest("Rewind returns every slot at once, in the same buffer", [this](auto& ls)
	{
		MultiPoolAllocator pool("ResultContainerTest");
		ResultContainer container(pool, TestCapacitySlots);

		std::uintptr_t firstAddress = 0;
		for (std::size_t index = 0; index < TestFilledSlots; ++index)
		{
			const auto slot = container.Append();
			FillSlotWithPattern(*slot, index);

			if (index == 0)
			{
				firstAddress = reinterpret_cast<std::uintptr_t>(slot);
			}
		}

		container.Rewind();

		ls << "After rewinding " << TestFilledSlots << " slots: count = " << container.GetCount()
		   << ", epoch = " << container.GetEpoch() << '.' << lf;

		if (container.GetCount() != 0)
		{
			ls << "A rewind left " << container.GetCount() << " slots still claimed." << lferr;
		}

		if (container.GetEpoch() != 1)
		{
			ls << "One rewind advanced the epoch to " << container.GetEpoch() << ", not to 1." << lferr;
		}

		if (reinterpret_cast<std::uintptr_t>(container.Append()) != firstAddress)
		{
			ls << "After a rewind the first slot is a different address, so the buffer was replaced rather than"
			   << " refilled - reuse is supposed to be a rewind, not a reallocation." << lferr;
		}

		if (container.GetEpoch() != 1)
		{
			ls << "A second run advanced the epoch to " << container.GetEpoch() << "; only a rewind may change it."
			   << lferr;
		}
	});

	AddTest("Every rewind advances the epoch by exactly one", [this](auto& ls)
	{
		MultiPoolAllocator pool("ResultContainerTest");
		ResultContainer container(pool, TestCapacitySlots);

		constexpr std::size_t rewinds = 2;
		for (std::size_t index = 0; index < rewinds; ++index)
		{
			(void) container.Append();
			container.Rewind();
		}

		ls << rewinds << " rewinds advanced the epoch to " << container.GetEpoch() << '.' << lf;

		if (container.GetEpoch() != rewinds)
		{
			ls << rewinds << " rewinds left the epoch at " << container.GetEpoch() << "; if rewinds are not counted"
			   << " individually then a reference to an old run cannot be recognised, which is the whole reason an"
			   << " index needs an epoch." << lferr;
		}
	});

	AddTest("Growing adds exactly the slots asked for and keeps what was written", [this](auto& ls)
	{
		MultiPoolAllocator pool("ResultContainerTest");
		ResultContainer container(pool, TestCapacitySlots);

		constexpr std::size_t filledSlots = 2;
		constexpr std::size_t grownBy = 3;
		for (std::size_t index = 0; index < filledSlots; ++index)
		{
			FillSlotWithPattern(*container.Append(), index);
		}

		container.Grow(grownBy);

		ls << "Growing by " << grownBy << " slots: capacity = " << container.GetCapacity()
		   << ", count = " << container.GetCount() << '.' << lf;

		if (container.GetCapacity() != TestCapacitySlots + grownBy)
		{
			ls << "Growing " << TestCapacitySlots << " slots by " << grownBy << " left the capacity at "
			   << container.GetCapacity() << "; a growth policy rounding the figure would leave a caller reasoning"
			   << " with a ceiling it was never given." << lferr;
		}

		if (container.GetCount() != filledSlots)
		{
			ls << "Growing changed the count from " << filledSlots << " to " << container.GetCount() << '.' << lferr;
		}

		for (std::size_t index = 0; index < filledSlots; ++index)
		{
			if (!SlotHoldsPattern(container.GetSlot(index), index))
			{
				ls << "Slot " << index << " no longer holds the bytes written to it before the grow, so growing the"
				   << " buffer discarded results that had already been produced." << lferr;
				break;
			}
		}

		const auto stride = reinterpret_cast<std::uintptr_t>(&container.GetSlot(1)) -
							reinterpret_cast<std::uintptr_t>(&container.GetSlot(0));
		if (stride != ResultContainer::SlotSizeBytes)
		{
			ls << "After the grow, slots 0 and 1 are " << stride << " bytes apart instead of "
			   << ResultContainer::SlotSizeBytes << ": the bytes were copied but not into fixed-size slots." << lferr;
		}

		if (container.Append() != &container.GetSlot(filledSlots))
		{
			ls << "After the grow the next append did not go to slot " << filledSlots
			   << ", so the write cursor was not carried over and the new slots overwrite results already in"
			   << " flight." << lferr;
		}
	});
}

} // namespace hbe
#endif // __UNIT_TEST__
