#pragma once

#include <algorithm>
#include <functional>
#include <utility>

#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"

namespace hbe
{

	template<typename TKey, typename TValue, class TCompare = std::less<TKey>, class TAllocator = DefaultAllocator<uint8_t>>
	class Map final
	{
	public:
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
			Iterator& operator++() { ++ptr; return *this; }
			bool operator==(const Iterator& rhs) const { return ptr == rhs.ptr; }
			bool operator!=(const Iterator& rhs) const { return ptr != rhs.ptr; }
			Pair& operator*() { return *ptr; }
			const Pair& operator*() const { return *ptr; }
			Pair* operator->() { return ptr; }
			const Pair* operator->() const { return ptr; }
		};

		using ConstIterator = Iterator;

	private:
		static constexpr TIndex DefaultCapacity = 4;

		Pair* entries;
		TIndex count;
		TIndex cap;
		TCompare compare;
		TAllocator allocator;

	public:
		Iterator begin() { return Iterator(entries); }
		Iterator end() { return Iterator(entries + count); }
		ConstIterator begin() const { return ConstIterator(entries); }
		ConstIterator end() const { return ConstIterator(entries + count); }

	public:
		Map(const Map&) = delete;
		Map& operator=(const Map&) = delete;

		Map() noexcept
			: entries(nullptr)
			, count(0)
			, cap(0)
		{
		}

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
			Release();
		}

		Map& operator=(Map&& rhs) noexcept
		{
			if (this != &rhs)
			{
				Release();

				entries = rhs.entries;
				count = rhs.count;
				cap = rhs.cap;

				rhs.entries = nullptr;
				rhs.count = 0;
				rhs.cap = 0;
			}

			return *this;
		}

		TValue& operator[](const TKey& key)
		{
			auto idx = FindIndex(key);
			if (idx >= 0)
			{
				return entries[idx].value;
			}

			idx = InsertSorted(key);
			return entries[idx].value;
		}

		TValue& operator[](TKey&& key)
		{
			auto idx = FindIndex(key);
			if (idx >= 0)
			{
				return entries[idx].value;
			}

			idx = InsertSorted(std::move(key));
			return entries[idx].value;
		}

		Iterator Find(const TKey& key)
		{
			auto idx = FindIndex(key);
			if (idx < 0)
				return end();

			return Iterator(entries + idx);
		}

		ConstIterator Find(const TKey& key) const
		{
			auto idx = FindIndex(key);
			if (idx < 0)
				return end();

			return ConstIterator(entries + idx);
		}

		bool Insert(const TKey& key, const TValue& value)
		{
			auto idx = FindIndex(key);
			if (idx >= 0)
				return false;

			idx = InsertSorted(key);
			entries[idx].value = value;
			return true;
		}

		bool Insert(const TKey& key, TValue&& value)
		{
			auto idx = FindIndex(key);
			if (idx >= 0)
				return false;

			idx = InsertSorted(key);
			entries[idx].value = std::move(value);
			return true;
		}

		bool Insert(TKey&& key, TValue&& value)
		{
			auto idx = FindIndex(key);
			if (idx >= 0)
				return false;

			idx = InsertSorted(std::move(key));
			entries[idx].value = std::move(value);
			return true;
		}

		bool Remove(const TKey& key)
		{
			auto idx = FindIndex(key);
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

		bool Contains(const TKey& key) const
		{
			return FindIndex(key) >= 0;
		}

		[[nodiscard]] TIndex Size() const { return count; }
		[[nodiscard]] bool IsEmpty() const { return count == 0; }

		void Clear()
		{
			for (TIndex i = 0; i < count; ++i)
			{
				entries[i].~Pair();
			}

			count = 0;
		}

	private:
		TIndex FindIndex(const TKey& key) const
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

		TIndex LowerBound(const TKey& key) const
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

		TIndex InsertSorted(const TKey& key)
		{
			if (count == cap)
			{
				Grow();
			}

			auto idx = LowerBound(key);

			for (TIndex i = count; i > idx; --i)
			{
				new (&entries[i]) Pair(std::move(entries[i - 1]));
				entries[i - 1].~Pair();
			}

			new (&entries[idx]) Pair{key, TValue{}};
			++count;

			return idx;
		}

		TIndex InsertSorted(TKey&& key)
		{
			if (count == cap)
			{
				Grow();
			}

			auto idx = LowerBound(key);

			for (TIndex i = count; i > idx; --i)
			{
				new (&entries[i]) Pair(std::move(entries[i - 1]));
				entries[i - 1].~Pair();
			}

			new (&entries[idx]) Pair{std::move(key), TValue{}};
			++count;

			return idx;
		}

		void Grow()
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

		void Release()
		{
			if (entries == nullptr)
				return;

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
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
