// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include <functional>
#include <utility>

#include "Core/CommonMacros.h"
#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"


namespace hbe
{

	template<typename TKey, typename TValue, class TCompare = std::less<TKey>, class TAllocator = DefaultAllocator<uint8_t>>
	class Map final
	{
	public:
		static constexpr int DefaultCapacity = 4;

		struct Pair final
		{
			TKey key;
			TValue value;
		};

		using TIndex = int;

		class Iterator
		{
		public:
			using TPair = Pair;
			friend class Map;

		private:
			Pair* ptr;

		public:
			explicit Iterator(Pair* ptr) noexcept : ptr(ptr) {}

			Iterator& operator++() noexcept { ++ptr; return *this; }
			bool operator==(const Iterator& rhs) const noexcept { return ptr == rhs.ptr; }
			bool operator!=(const Iterator& rhs) const noexcept { return ptr != rhs.ptr; }
			Pair& operator*() noexcept { return *ptr; }
			const Pair& operator*() const noexcept { return *ptr; }
			Pair* operator->() noexcept { return ptr; }
			const Pair* operator->() const noexcept { return ptr; }
		};

		using ConstIterator = Iterator;

		Map() noexcept
			: entries(nullptr)
			, count(0)
			, cap(0)
		{
		}

		Map(const Map&) = delete;

		Map(Map&& rhs) noexcept
			: entries(rhs.entries)
			, count(rhs.count)
			, cap(rhs.cap)
		{
			rhs.entries = nullptr;
			rhs.count = 0;
			rhs.cap = 0;
		}

		~Map()
		{
			release();
		}

		Map& operator=(const Map&) = delete;

		Map& operator=(Map&& rhs) noexcept
		{
			if (this != &rhs)
			{
				release();

				entries = rhs.entries;
				count = rhs.count;
				cap = rhs.cap;

				rhs.entries = nullptr;
				rhs.count = 0;
				rhs.cap = 0;
			}

			return *this;
		}

		Iterator begin() noexcept { return Iterator(entries); }
		Iterator end() noexcept { return Iterator(entries + count); }
		ConstIterator begin() const noexcept { return ConstIterator(entries); }
		ConstIterator end() const noexcept { return ConstIterator(entries + count); }

		TValue& operator[](const TKey& key)
		{
			auto idx = findIndex(key);
			if (idx >= 0)
			{
				return entries[idx].value;
			}

			idx = insertSorted(key);
			return entries[idx].value;
		}

		TValue& operator[](TKey&& key)
		{
			auto idx = findIndex(key);
			if (idx >= 0)
			{
				return entries[idx].value;
			}

			idx = insertSorted(std::move(key));
			return entries[idx].value;
		}

		[[nodiscard]] Iterator find(const TKey& key) noexcept
		{
			auto idx = findIndex(key);
			if (idx < 0)
				return end();

			return Iterator(entries + idx);
		}

		[[nodiscard]] ConstIterator find(const TKey& key) const noexcept
		{
			auto idx = findIndex(key);
			if (idx < 0)
				return end();

			return ConstIterator(entries + idx);
		}

		bool insert(const TKey& key, const TValue& value)
		{
			auto idx = findIndex(key);
			if (idx >= 0)
				return false;

			idx = insertSorted(key);
			entries[idx].value = value;

			return true;
		}

		bool insert(const TKey& key, TValue&& value)
		{
			auto idx = findIndex(key);
			if (idx >= 0)
				return false;

			idx = insertSorted(key);
			entries[idx].value = std::move(value);

			return true;
		}

		bool insert(TKey&& key, TValue&& value)
		{
			auto idx = findIndex(key);
			if (idx >= 0)
				return false;

			idx = insertSorted(std::move(key));
			entries[idx].value = std::move(value);

			return true;
		}

		bool Remove(const TKey& key)
		{
			auto idx = findIndex(key);
			if (idx < 0)
				return false;

			entries[idx].~Pair();

			for (TIndex i = idx + 1; i < count; ++i)
			{
				new (&entries[i - 1]) Pair(std::move(entries[i]));
				entries[i].~Pair();
			}

			--count;

			return true;
		}

		[[nodiscard]] bool contains(const TKey& key) const noexcept
		{
			return findIndex(key) >= 0;
		}

		[[nodiscard]] TIndex Size() const noexcept { return count; }
		[[nodiscard]] bool IsEmpty() const noexcept { return count == 0; }

		void clear() noexcept
		{
			for (TIndex i = 0; i < count; ++i)
			{
				entries[i].~Pair();
			}

			count = 0;
		}

	private:
		Pair* entries;
		TIndex count;
		TIndex cap;
		TCompare compare;
		TAllocator allocator;

		[[nodiscard]] TIndex findIndex(const TKey& key) const noexcept
		{
			if (count == 0)
				return -1;

			TIndex lo = 0;
			TIndex hi = count - 1;

			while (lo <= hi)
			{
				TIndex mid = lo + (hi - lo) / 2;

				if (compare(entries[mid].key, key))
				{
					lo = mid + 1;
				}
				else if (compare(key, entries[mid].key))
				{
					hi = mid - 1;
				}
				else
				{
					return mid;
				}
			}

			return -1;
		}

		[[nodiscard]] TIndex lowerBound(const TKey& key) const noexcept
		{
			TIndex lo = 0;
			TIndex hi = count;

			while (lo < hi)
			{
				TIndex mid = lo + (hi - lo) / 2;

				if (compare(entries[mid].key, key))
				{
					lo = mid + 1;
				}
				else
				{
					hi = mid;
				}
			}

			return lo;
		}

		TIndex insertSorted(const TKey& key) noexcept
		{
			if (count == cap)
			{
				grow();
			}

			auto idx = lowerBound(key);

			for (TIndex i = count; i > idx; --i)
			{
				new (&entries[i]) Pair(std::move(entries[i - 1]));
				entries[i - 1].~Pair();
			}

			new (&entries[idx]) Pair{key, TValue{}};
			++count;

			return idx;
		}

		TIndex insertSorted(TKey&& key) noexcept
		{
			if (count == cap)
			{
				grow();
			}

			auto idx = lowerBound(key);

			for (TIndex i = count; i > idx; --i)
			{
				new (&entries[i]) Pair(std::move(entries[i - 1]));
				entries[i - 1].~Pair();
			}

			new (&entries[idx]) Pair{std::move(key), TValue{}};
			++count;

			return idx;
		}

		void grow() noexcept
		{
			auto newCap = std::max(DefaultCapacity, cap * 2);
			auto allocSize = sizeof(Pair) * newCap;
			auto* raw = allocator.allocate(allocSize);
			auto* newEntries = reinterpret_cast<Pair*>(raw);

			for (TIndex i = 0; i < count; ++i)
			{
				new (&newEntries[i]) Pair(std::move(entries[i]));
				entries[i].~Pair();
			}

			if (entries != nullptr)
			{
				auto oldSize = sizeof(Pair) * cap;
				allocator.deallocate(reinterpret_cast<uint8_t*>(entries), oldSize);
			}

			entries = newEntries;
			cap = newCap;
		}

		void release() noexcept
		{
			returnIf(entries == nullptr);

			for (TIndex i = 0; i < count; ++i)
			{
				entries[i].~Pair();
			}

			auto allocSize = sizeof(Pair) * cap;
			allocator.deallocate(reinterpret_cast<uint8_t*>(entries), allocSize);

			entries = nullptr;
			count = 0;
			cap = 0;
		}
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class MapTest : public TestCollection
	{
	public:
		MapTest() : TestCollection("MapTest") {}

	protected:
		void prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
