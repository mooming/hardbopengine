// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>

#include "Core/Debug.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/Memory.h"


namespace hbe
{

	enum class EHashEntryState : uint8_t
	{
		Empty,
		Occupied,
		Tombstone
	};

	template<typename TKey, typename TValue, class THash = std::hash<TKey>, class TKeyEqual = std::equal_to<TKey>,
		class TAllocator = DefaultAllocator<uint8_t>>
	class HashMap final
	{
	public:
		using TIndex = int;

		static constexpr TIndex MinCapacity = 4;
		static constexpr double MaxLoadFactor = 0.7;

		struct Pair final
		{
			TKey key;
			TValue value;
		};

		class Iterator
		{
		public:
			using TMap = HashMap;
			friend class HashMap;

		private:
			TMap* map;
			TIndex index;

		public:
			explicit Iterator(TMap* map, TIndex index) noexcept
				: map(map)
				, index(index)
			{
			}

			Iterator& operator++() noexcept
			{
				FatalAssert(map != nullptr);
				FatalAssert(index >= 0);
				++index;

				while (index < map->cap && map->states[index] != EHashEntryState::Occupied)
				{
					++index;
				}

				return *this;
			}

			bool operator==(const Iterator& rhs) const noexcept
			{
				return index == rhs.index && map == rhs.map;
			}

			bool operator!=(const Iterator& rhs) const noexcept
			{
				return !(*this == rhs);
			}

			Pair& operator*() noexcept
			{
				FatalAssert(map != nullptr && map->IsValidSlot(index));
				return map->entries[index];
			}

			const Pair& operator*() const noexcept
			{
				FatalAssert(map != nullptr && map->IsValidSlot(index));
				return map->entries[index];
			}

			Pair* operator->() noexcept { return &(**this); }
			const Pair* operator->() const noexcept { return &(**this); }
		};

		class ConstIterator
		{
		public:
			using TMap = HashMap;
			friend class HashMap;

		private:
			const TMap* map;
			TIndex index;

		public:
			explicit ConstIterator(const TMap* map, TIndex index) noexcept
				: map(map)
				, index(index)
			{
			}

			ConstIterator& operator++() noexcept
			{
				FatalAssert(map != nullptr);
				FatalAssert(index >= 0);
				++index;

				while (index < map->cap && map->states[index] != EHashEntryState::Occupied)
				{
					++index;
				}

				return *this;
			}

			bool operator==(const ConstIterator& rhs) const noexcept
			{
				return index == rhs.index && map == rhs.map;
			}

			bool operator!=(const ConstIterator& rhs) const noexcept
			{
				return !(*this == rhs);
			}

			const Pair& operator*() const noexcept
			{
				FatalAssert(map != nullptr && map->IsValidSlot(index));
				return map->entries[index];
			}

			const Pair* operator->() const noexcept { return &(**this); }
		};

		HashMap() noexcept
			: cap(0)
			, count(0)
			, tombstoneCount(0)
			, entries(nullptr)
			, states(nullptr)
		{
		}

		HashMap(const HashMap&) = delete;

		explicit HashMap(TIndex initialCapacity)
			: cap(0)
			, count(0)
			, tombstoneCount(0)
			, entries(nullptr)
			, states(nullptr)
		{
			if (initialCapacity > 0)
			{
				Reserve(initialCapacity);
			}
		}

		HashMap(HashMap&& rhs) noexcept
			: cap(rhs.cap)
			, count(rhs.count)
			, tombstoneCount(rhs.tombstoneCount)
			, entries(rhs.entries)
			, states(rhs.states)
			, hash(std::move(rhs.hash))
			, keyEqual(std::move(rhs.keyEqual))
		{
			rhs.cap = 0;
			rhs.count = 0;
			rhs.tombstoneCount = 0;
			rhs.entries = nullptr;
			rhs.states = nullptr;
		}

		~HashMap() noexcept { Release(); }

		HashMap& operator=(const HashMap&) = delete;

		HashMap& operator=(HashMap&& rhs) noexcept
		{
			if (this != &rhs)
			{
				Release();

				cap = rhs.cap;
				count = rhs.count;
				tombstoneCount = rhs.tombstoneCount;
				entries = rhs.entries;
				states = rhs.states;
				hash = std::move(rhs.hash);
				keyEqual = std::move(rhs.keyEqual);

				rhs.cap = 0;
				rhs.count = 0;
				rhs.tombstoneCount = 0;
				rhs.entries = nullptr;
				rhs.states = nullptr;
			}

			return *this;
		}

		Iterator begin() noexcept
		{
			TIndex first = 0;
			while (first < cap && states[first] != EHashEntryState::Occupied)
			{
				++first;
			}

			return Iterator(this, first);
		}

		Iterator end() noexcept { return Iterator(this, cap); }

		ConstIterator begin() const noexcept
		{
			TIndex first = 0;
			while (first < cap && states[first] != EHashEntryState::Occupied)
			{
				++first;
			}

			return ConstIterator(this, first);
		}

		ConstIterator end() const noexcept { return ConstIterator(this, cap); }

		TValue& operator[](const TKey& key)
		{
			auto idx = FindSlot(key);
			if (idx >= 0 && states[idx] == EHashEntryState::Occupied)
			{
				return entries[idx].value;
			}

			auto [inserted, slot] = InsertInternal(key);
			FatalAssert(inserted);
			return entries[slot].value;
		}

		TValue& operator[](TKey&& key)
		{
			auto idx = FindSlot(key);
			if (idx >= 0 && states[idx] == EHashEntryState::Occupied)
			{
				return entries[idx].value;
			}

			auto [inserted, slot] = InsertInternal(std::move(key));
			FatalAssert(inserted);
			return entries[slot].value;
		}

		[[nodiscard]] Iterator Find(const TKey& key) noexcept
		{
			auto idx = FindSlot(key);
			if (idx < 0 || states[idx] != EHashEntryState::Occupied)
				return end();

			return Iterator(this, idx);
		}

		[[nodiscard]] ConstIterator Find(const TKey& key) const noexcept
		{
			auto idx = FindSlot(key);
			if (idx < 0 || states[idx] != EHashEntryState::Occupied)
				return end();

			return ConstIterator(this, idx);
		}

		bool Insert(const TKey& key, const TValue& value)
		{
			auto idx = FindSlot(key);
			if (idx >= 0 && states[idx] == EHashEntryState::Occupied)
				return false;

			auto [inserted, slot] = InsertInternal(key);
			if (inserted)
			{
				entries[slot].value = value;
			}

			return inserted;
		}

		bool Insert(const TKey& key, TValue&& value)
		{
			auto idx = FindSlot(key);
			if (idx >= 0 && states[idx] == EHashEntryState::Occupied)
				return false;

			auto [inserted, slot] = InsertInternal(key);
			if (inserted)
			{
				entries[slot].value = std::move(value);
			}

			return inserted;
		}

		bool Insert(TKey&& key, TValue&& value)
		{
			auto idx = FindSlot(key);
			if (idx >= 0 && states[idx] == EHashEntryState::Occupied)
				return false;

			auto [inserted, slot] = InsertInternal(std::move(key));
			if (inserted)
			{
				entries[slot].value = std::move(value);
			}

			return inserted;
		}

		bool Remove(const TKey& key)
		{
			auto idx = FindSlot(key);
			if (idx < 0 || states[idx] != EHashEntryState::Occupied)
				return false;

			entries[idx].~Pair();
			states[idx] = EHashEntryState::Tombstone;
			--count;
			++tombstoneCount;

			return true;
		}

		[[nodiscard]] bool Contains(const TKey& key) const noexcept
		{
			auto idx = FindSlot(key);
			return idx >= 0 && states[idx] == EHashEntryState::Occupied;
		}

		[[nodiscard]] TIndex Size() const noexcept { return count; }
		[[nodiscard]] TIndex Capacity() const noexcept { return cap; }
		[[nodiscard]] bool IsEmpty() const noexcept { return count == 0; }

		void Clear() noexcept
		{
			for (TIndex i = 0; i < cap; ++i)
			{
				if (states[i] == EHashEntryState::Occupied)
				{
					entries[i].~Pair();
				}

				states[i] = EHashEntryState::Empty;
			}

			count = 0;
			tombstoneCount = 0;
		}

		void Reserve(TIndex newCapacity)
		{
			if (newCapacity <= cap)
				return;

			auto newCap = std::max(MinCapacity, cap);
			while (newCap < newCapacity)
			{
				newCap *= 2;
			}

			Rehash(newCap);
		}

	private:
		TIndex cap;
		TIndex count;
		TIndex tombstoneCount;
		Pair* entries;
		EHashEntryState* states;
		THash hash;
		TKeyEqual keyEqual;
		TAllocator allocator;

		[[nodiscard]] bool IsValidSlot(TIndex index) const noexcept
		{
			return index >= 0 && index < cap && states[index] == EHashEntryState::Occupied;
		}

		TIndex FindSlot(const TKey& key) const noexcept
		{
			if (cap == 0)
				return -1;

			auto h = hash(key) & static_cast<std::size_t>(cap - 1);
			auto idx = static_cast<TIndex>(h);

			for (TIndex probe = 0; probe < cap; ++probe)
			{
				auto slot = (idx + probe) & (cap - 1);

				if (states[slot] == EHashEntryState::Empty)
					return -1;

				if (states[slot] == EHashEntryState::Occupied && keyEqual(entries[slot].key, key))
					return slot;
			}

			return -1;
		}

		std::pair<bool, TIndex> InsertInternal(const TKey& key) noexcept
		{
			if (ShouldGrow())
			{
				Grow();
			}

			auto h = hash(key) & static_cast<std::size_t>(cap - 1);
			auto idx = static_cast<TIndex>(h);

			for (TIndex probe = 0; probe < cap; ++probe)
			{
				auto slot = (idx + probe) & (cap - 1);

				if (states[slot] == EHashEntryState::Occupied && keyEqual(entries[slot].key, key))
				{
					return {false, slot};
				}

				if (states[slot] != EHashEntryState::Occupied)
				{
					if (states[slot] == EHashEntryState::Tombstone)
					{
						--tombstoneCount;
					}

					new (&entries[slot]) Pair{key, TValue{}};
					states[slot] = EHashEntryState::Occupied;
					++count;

					return {true, slot};
				}
			}

			FatalAssert(false, "HashMap should have grown before reaching full capacity");

			return {false, -1};
		}

		std::pair<bool, TIndex> InsertInternal(TKey&& key) noexcept
		{
			if (ShouldGrow())
			{
				Grow();
			}

			auto h = hash(key) & static_cast<std::size_t>(cap - 1);
			auto idx = static_cast<TIndex>(h);

			for (TIndex probe = 0; probe < cap; ++probe)
			{
				auto slot = (idx + probe) & (cap - 1);

				if (states[slot] == EHashEntryState::Occupied && keyEqual(entries[slot].key, key))
				{
					return {false, slot};
				}

				if (states[slot] != EHashEntryState::Occupied)
				{
					if (states[slot] == EHashEntryState::Tombstone)
					{
						--tombstoneCount;
					}

					new (&entries[slot]) Pair{std::move(key), TValue{}};
					states[slot] = EHashEntryState::Occupied;
					++count;

					return {true, slot};
				}
			}

			FatalAssert(false, "HashMap should have grown before reaching full capacity");

			return {false, -1};
		}

		[[nodiscard]] bool ShouldGrow() const noexcept
		{
			auto totalUsed = count + tombstoneCount;
			return cap == 0 || static_cast<double>(totalUsed) >= static_cast<double>(cap) * MaxLoadFactor;
		}

		void Grow() noexcept
		{
			auto newCap = std::max(MinCapacity, cap * 2);
			Rehash(newCap);
		}

		void Rehash(TIndex newCapacity) noexcept
		{
			auto allocSize = sizeof(Pair) * newCapacity + sizeof(EHashEntryState) * newCapacity;
			auto* raw = allocator.allocate(allocSize);
			auto* newEntries = reinterpret_cast<Pair*>(raw);
			auto* newStates = reinterpret_cast<EHashEntryState*>(newEntries + newCapacity);

			for (TIndex i = 0; i < newCapacity; ++i)
			{
				newStates[i] = EHashEntryState::Empty;
			}

			auto oldEntries = entries;
			auto oldStates = states;
			auto oldCapacity = cap;

			entries = newEntries;
			states = newStates;
			cap = newCapacity;
			count = 0;
			tombstoneCount = 0;

			for (TIndex i = 0; i < oldCapacity; ++i)
			{
				if (oldStates[i] == EHashEntryState::Occupied)
				{
					auto h = hash(oldEntries[i].key) & static_cast<std::size_t>(newCapacity - 1);
					auto idx = static_cast<TIndex>(h);

					for (TIndex probe = 0; probe < newCapacity; ++probe)
					{
						auto slot = (idx + probe) & (newCapacity - 1);

						if (newStates[slot] == EHashEntryState::Empty)
						{
							new (&entries[slot]) Pair(std::move(oldEntries[i]));
							newStates[slot] = EHashEntryState::Occupied;
							++count;
							break;
						}
					}

					oldEntries[i].~Pair();
				}
			}

			if (oldEntries != nullptr)
			{
				auto oldAllocSize = sizeof(Pair) * oldCapacity + sizeof(EHashEntryState) * oldCapacity;
				allocator.deallocate(reinterpret_cast<uint8_t*>(oldEntries), oldAllocSize);
			}
		}

		void Release() noexcept
		{
			if (entries == nullptr)
				return;

			for (TIndex i = 0; i < cap; ++i)
			{
				if (states[i] == EHashEntryState::Occupied)
				{
					entries[i].~Pair();
				}
			}

			auto allocSize = sizeof(Pair) * cap + sizeof(EHashEntryState) * cap;
			allocator.deallocate(reinterpret_cast<uint8_t*>(entries), allocSize);

			entries = nullptr;
			states = nullptr;
			cap = 0;
			count = 0;
			tombstoneCount = 0;
		}
	};

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class HashMapTest : public TestCollection
	{
	public:
		HashMapTest() : TestCollection("HashMapTest") {}

	protected:
		void Prepare() override;
	};

} // namespace hbe
#endif //__UNIT_TEST__
