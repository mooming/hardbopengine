// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "Container/Deque.h"
#include "Core/Debug.h"


namespace hbe
{
/// @brief A bounded priority queue using bucket-based approach.
/// @details One deque per priority value, indexed by priority (0-255). The most urgent priority is the
///          HIGHEST number, and within one priority items drain oldest-first, so two items sharing a
///          priority run in arrival order rather than newest-first.
/// @details Buckets exist only while they hold something: a priority's deque is created on the first
///          push to it and released when it empties. Constructing a queue therefore allocates nothing
///          no matter how many priority levels it declares, which is what makes MaxPriority=256 and a
///          roomy BucketSizeHint affordable at the same time.
/// @tparam T Task type must have uint8_t priority and HasFinished() method.
/// @tparam MaxPriority Number of priority levels; also the highest meaningful priority value.
/// @tparam BucketSizeHint Capacity a bucket is created with, for a priority known to be busy. Zero
///          leaves the deque to its own default. Only priorities actually used pay this, so setting it
///          costs nothing on the levels a stream never uses.
template <typename T, std::size_t MaxPriority = 256, std::size_t BucketSizeHint = 0>
class BoundedPriorityQueue final
{
	using TBucket = Deque<T>;
	using TBuckets = std::array<std::optional<TBucket>, MaxPriority>;

	TBuckets buckets;
	std::size_t totalSize;
	std::size_t highestBucket;

	/// @brief Get the deque for a priority, creating it on first use.
	[[nodiscard]] TBucket& AcquireBucket(std::size_t priority) noexcept
	{
		FatalAssert(priority < MaxPriority, "BoundedPriorityQueue priority is not a bucket index");

		auto& bucket = buckets[priority];
		if (!bucket.has_value())
		{
			if constexpr (BucketSizeHint > 0)
			{
				bucket.emplace(static_cast<typename TBucket::TIndex>(BucketSizeHint));
			}
			else
			{
				bucket.emplace();
			}
		}

		return *bucket;
	}

	/// @brief Release a bucket that has run dry, so its storage is not held across an idle period.
	/// @note The engaged-means-non-empty invariant is maintained here and only here; Pop and Remove
	///       rely on it to treat has_value() as "there is work at this priority".
	void ReleaseEmptyBucket(std::size_t priority) noexcept
	{
		auto& bucket = buckets[priority];
		if (bucket.has_value() && bucket->IsEmpty())
		{
			bucket.reset();
		}
	}

	/// @brief Walk the tracked priority down to the next one that still holds items.
	/// @note Ends at 0 with nothing there once the queue is empty, which is harmless because every
	///       reader checks totalSize first. Cost is paid when a queue empties, not per pop.
	void RetrackHighestBucket() noexcept
	{
		while (highestBucket > 0 && !buckets[highestBucket].has_value())
		{
			--highestBucket;
		}
	}

public:
	BoundedPriorityQueue() noexcept
		: totalSize(0)
		, highestBucket(0)
	{
	}

	BoundedPriorityQueue(const BoundedPriorityQueue&) = delete;
	BoundedPriorityQueue(BoundedPriorityQueue&&) = delete;
	~BoundedPriorityQueue() = default;

	BoundedPriorityQueue& operator=(const BoundedPriorityQueue&) = delete;
	BoundedPriorityQueue& operator=(BoundedPriorityQueue&&) = delete;

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		return totalSize == 0;
	}

	[[nodiscard]] std::size_t Size() const noexcept
	{
		return totalSize;
	}

	void Push(const T& item) noexcept
	{
		const auto priority = static_cast<std::size_t>(item.priority);
		AcquireBucket(priority).PushBack(item);
		++totalSize;

		if (priority > highestBucket)
			highestBucket = priority;
	}

	void Push(T&& item) noexcept
	{
		const auto priority = static_cast<std::size_t>(item.priority);
		AcquireBucket(priority).PushBack(std::move(item));
		++totalSize;

		if (priority > highestBucket)
			highestBucket = priority;
	}

	/// @brief Take the most urgent item, which is the oldest one at the highest populated priority.
	/// @brief Serve the most urgent item, reporting its `priority` as the level it was actually served from.
	/// @details **The bucket index is the priority**, not the byte. `Push` files an item by the byte and nothing
	/// afterwards
	///          re-sorts it, so a byte written while the item is queued leaves it filed under the old level while every
	///          read reports the new one. Rather than hand a caller an item whose two accounts disagree, `Pop`
	///          overwrites the byte with the level it served from. To change a queued item's priority, remove it and
	///          push it again: assignment cannot repair an order, only relocate the item.
	[[nodiscard]] std::optional<T> Pop() noexcept
	{
		if (totalSize == 0)
			return std::nullopt;

		auto& bucket = *buckets[highestBucket];
		const std::size_t servedFromLevel = highestBucket;
		auto item = std::move(bucket.Front());
		bucket.PopFront();
		--totalSize;

		ReleaseEmptyBucket(servedFromLevel);
		RetrackHighestBucket();

		item.priority = static_cast<decltype(item.priority)>(servedFromLevel);

		return item;
	}

	/// @brief Look at the most urgent item without removing it.
	[[nodiscard]] std::optional<T> Top() const noexcept
	{
		if (totalSize == 0)
			return std::nullopt;

		return buckets[highestBucket]->Front();
	}

	using TPredicate = bool (*)(const T&);

	/// @brief Drop every item the predicate claims, returning how many went.
	/// @details Each bucket is rotate-filtered: every item is taken from the front exactly once and
	///          either dropped or pushed to the back, so survivors keep their arrival order without
	///          needing a second container to compact into.
	std::size_t Remove(TPredicate predicate) noexcept
	{
		if (predicate == nullptr)
			return 0;

		std::size_t removed = 0;
		for (std::size_t priority = 0; priority < MaxPriority; ++priority)
		{
			auto& bucket = buckets[priority];
			if (!bucket.has_value())
				continue;

			const auto numItems = static_cast<std::size_t>(bucket->Size());
			for (std::size_t index = 0; index < numItems; ++index)
			{
				auto item = std::move(bucket->Front());
				bucket->PopFront();

				if (predicate(item))
				{
					++removed;
				}
				else
				{
					bucket->PushBack(std::move(item));
				}
			}

			ReleaseEmptyBucket(priority);
		}

		totalSize -= removed;
		RetrackHighestBucket();

		return removed;
	}

	template <typename Iterator>
	void PushRange(Iterator begin, Iterator end) noexcept
	{
		for (auto it = begin; it != end; ++it)
		{
			Push(*it);
		}
	}

	template <typename TContainer>
	void PushRange(const TContainer& container) noexcept
	{
		PushRange(container.begin(), container.end());
	}

	void Clear() noexcept
	{
		for (auto& bucket : buckets)
		{
			bucket.reset();
		}

		totalSize = 0;
		highestBucket = 0;
	}
};
} // namespace hbe

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class BoundedPriorityQueueTest : public TestCollection
{
public:
	BoundedPriorityQueueTest()
		: TestCollection("BoundedPriorityQueueTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif // TEST_ENABLED
