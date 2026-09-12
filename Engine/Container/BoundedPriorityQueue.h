// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

#include "HSTL/HVector.h"

namespace hbe
{

	/// @brief A bounded priority queue using bucket-based approach.
	/// @details Uses an array of vectors indexed by priority value (0-255).
	/// Provides O(1) insertion and O(1) extraction of highest priority task.
	/// Tracks lowest non-empty bucket for O(1) pop operations.
	/// Ideal when priority range is known and bounded.
	/// @tparam T Task type must have uint8_t priority and HasFinished() method.
	template<typename T, std::size_t MaxPriority = 256, std::size_t BucketSizeHint = 0>
	class BoundedPriorityQueue final
	{
		using TBuckets = std::array<HVector<T>, MaxPriority>;

		TBuckets buckets;
		std::size_t totalSize;
		std::size_t lowestBucket;

	public:
		BoundedPriorityQueue() noexcept
			: totalSize(0)
			, lowestBucket(MaxPriority)
		{
			for (auto& bucket : buckets)
			{
				bucket.reserve(BucketSizeHint);
			}
		}

		BoundedPriorityQueue(const BoundedPriorityQueue&) = delete;
		BoundedPriorityQueue(BoundedPriorityQueue&&) = delete;
		~BoundedPriorityQueue() = default;

		BoundedPriorityQueue& operator=(const BoundedPriorityQueue&) = delete;
		BoundedPriorityQueue& operator=(BoundedPriorityQueue&&) = delete;

		[[nodiscard]] bool IsEmpty() const noexcept { return totalSize == 0; }
		[[nodiscard]] std::size_t Size() const noexcept { return totalSize; }

		void Push(const T& item) noexcept
		{
			const auto priority = static_cast<std::size_t>(item.priority);
			buckets[priority].push_back(item);
			++totalSize;

			if (priority < lowestBucket)
				lowestBucket = priority;
		}

		void Push(T&& item) noexcept
		{
			const auto priority = static_cast<std::size_t>(item.priority);
			buckets[priority].emplace_back(std::move(item));
			++totalSize;

			if (priority < lowestBucket)
				lowestBucket = priority;
		}

		[[nodiscard]] std::optional<T> Pop() noexcept
		{
			if (totalSize == 0)
				return std::nullopt;

			auto& bucket = buckets[lowestBucket];
			auto item = bucket.back();
			bucket.pop_back();
			--totalSize;

			if (bucket.empty())
			{
				while (lowestBucket < MaxPriority && buckets[lowestBucket].empty())
					++lowestBucket;
			}

			return item;
		}

		[[nodiscard]] std::optional<T> Top() const noexcept
		{
			if (totalSize == 0)
				return std::nullopt;

			return buckets[lowestBucket].back();
		}

		using TPredicate = bool (*)(const T&);
		std::size_t Remove(TPredicate predicate) noexcept
		{
			if (predicate == nullptr)
				return 0;

			std::size_t removed = 0;
			for (auto i = lowestBucket; i < MaxPriority; ++i)
			{
				auto& bucket = buckets[i];
				const std::size_t numItems = bucket.size();
				bucket.erase(std::remove_if(bucket.begin(), bucket.end(), predicate), bucket.end());
				removed += (numItems - bucket.size());
			}

			totalSize -= removed;

			if (removed > 0)
			{
				while (lowestBucket < MaxPriority && buckets[lowestBucket].empty())
					++lowestBucket;
			}

			return removed;
		}

		template<typename Iterator>
		void PushRange(Iterator begin, Iterator end) noexcept
		{
			for (auto it = begin; it != end; ++it)
			{
				Push(*it);
			}
		}

		template<typename TContainer>
		void PushRange(const TContainer& container) noexcept
		{
			PushRange(container.begin(), container.end());
		}

		void Clear() noexcept
		{
			for (auto& bucket : buckets)
			{
				bucket.clear();
			}

			totalSize = 0;
			lowestBucket = MaxPriority;
		}
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class BoundedPriorityQueueTest : public TestCollection
	{
	public:
		BoundedPriorityQueueTest() : TestCollection("BoundedPriorityQueueTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif // __UNIT_TEST__
