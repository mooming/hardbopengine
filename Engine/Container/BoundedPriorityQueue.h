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
/// API reference: docs/Container/BoundedPriorityQueue/index.html
template <typename T, std::size_t MaxPriority = 256, std::size_t BucketSizeHint = 0>
class BoundedPriorityQueue final
{
	using TBucket = Deque<T>;
	using TBuckets = std::array<std::optional<TBucket>, MaxPriority>;
	using TPredicate = bool (*)(const T&);

	TBuckets buckets;
	std::size_t totalSize;
	std::size_t highestBucket;

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

	[[nodiscard]] std::optional<T> Top() const noexcept
	{
		if (totalSize == 0)
			return std::nullopt;

		return buckets[highestBucket]->Front();
	}

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

private:
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

	void ReleaseEmptyBucket(std::size_t priority) noexcept
	{
		auto& bucket = buckets[priority];
		if (bucket.has_value() && bucket->IsEmpty())
		{
			bucket.reset();
		}
	}

	void RetrackHighestBucket() noexcept
	{
		while (highestBucket > 0 && !buckets[highestBucket].has_value())
		{
			--highestBucket;
		}
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
