// Created by mooming.go@gmail.com

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
	/// Ideal when priority range is known and bounded.
	/// @tparam T Task type must have uint8_t priority and HasFinished() method.
	template<typename T, std::size_t MaxPriority = 256>
	class BoundedPriorityQueue final
	{
		static_assert(MaxPriority <= 256, "Priority must fit in uint8_t");

		using TBuckets = std::array<HVector<T>, MaxPriority>;

		TBuckets buckets;
		std::size_t totalSize;

	public:
		BoundedPriorityQueue()
		{
			for (auto& bucket : buckets)
			{
				bucket.reserve(4);
			}

			totalSize = 0;
		}

		BoundedPriorityQueue(const BoundedPriorityQueue&) = delete;
		BoundedPriorityQueue(BoundedPriorityQueue&&) = delete;
		~BoundedPriorityQueue() = default;

		BoundedPriorityQueue& operator=(const BoundedPriorityQueue&) = delete;
		BoundedPriorityQueue& operator=(BoundedPriorityQueue&&) = delete;

		[[nodiscard]] bool IsEmpty() const { return totalSize == 0; }
		[[nodiscard]] std::size_t Size() const { return totalSize; }

		void Push(const T& item)
		{
			const auto priority = static_cast<std::size_t>(item.priority);
			buckets[priority].push_back(item);
			++totalSize;
		}

		void Push(T&& item)
		{
			const auto priority = static_cast<std::size_t>(item.priority);
			buckets[priority].emplace_back(std::move(item));
			++totalSize;
		}

		[[nodiscard]] std::optional<T> Pop()
		{
			if (totalSize == 0)
				return std::nullopt;

			for (auto& bucket : buckets)
			{
				if (bucket.empty())
					continue;

				auto item = bucket.back();
				bucket.pop_back();
				--totalSize;

				return item; // NRVO
			}

			return std::nullopt;
		}

		[[nodiscard]] std::optional<T> Top() const
		{
			if (totalSize == 0)
				return std::nullopt;

			for (const auto& bucket : buckets)
			{
				if (!bucket.empty())
					return bucket.back();
			}

			return std::nullopt;
		}

		using TPredicate = bool (*)(const T&);
		std::size_t Remove(TPredicate predicate)
		{
			if (predicate == nullptr)
				return 0;

			std::size_t removed = 0;
			for (auto& bucket : buckets)
			{
				const std::size_t numItems = bucket.size();
				auto newEnd = std::erase_if(bucket.begin(), bucket.end(), predicate);
				removed += (numItems - bucket.size());
			}

			totalSize -= removed;

			return removed;
		}

		template<typename Iterator>
		void PushRange(Iterator begin, Iterator end)
		{
			for (auto it = begin; it != end; ++it)
			{
				Push(*it);
			}
		}

		template<typename TContainer>
		void PushRange(const TContainer& container)
		{
			PushRange(container.begin(), container.end());
		}

		void Clear()
		{
			for (auto& bucket : buckets)
			{
				bucket.clear();
			}

			totalSize = 0;
		}
	};

} // namespace hbe
