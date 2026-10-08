// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdlib>

#include "AllocatorID.h"
#include "AllocatorScope.h"
#include "Core/Debug.h"
#include "MemoryManager.h"


namespace hbe
{
/// API reference: docs/Memory/DefaultAllocator/index.html
template <typename T>
class DefaultAllocator final
{
public:
	using value_type = T;

	template <class TOther>
	struct rebind
	{
		using other = DefaultAllocator<TOther>;
	};

private:
	TAllocatorID allocatorID;

public:
	DefaultAllocator()
		: allocatorID(MemoryManager::GetCurrentAllocatorID())
	{
	}

	template <class TOther>
	explicit DefaultAllocator(const DefaultAllocator<TOther>& rhs) noexcept
		: allocatorID(rhs.GetSourceAllocatorID())
	{
	}

	[[nodiscard]] T* allocate(std::size_t n) noexcept
	{
		if (allocatorID == MemoryManager::SystemAllocatorID)
		{
			return static_cast<T*>(malloc(n * sizeof(T)));
		}

		AllocatorScope scope(allocatorID);
		auto& mmgr = MemoryManager::GetInstance();
		auto ptr = mmgr.AllocateByType<T>(n);

		return ptr;
	}

	void deallocate(T* ptr, std::size_t n) noexcept
	{
		Assert(ptr != nullptr);

		if (allocatorID == MemoryManager::SystemAllocatorID)
		{
			free(ptr);

			return;
		}

		AllocatorScope scope(allocatorID);
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.DeallocateTypes(ptr, n);
	}

	template <class TOther>
	bool operator==(const DefaultAllocator<TOther>& rhs) const noexcept
	{
		return allocatorID == rhs.allocatorID;
	}

	template <class TOther>
	bool operator!=(const DefaultAllocator<TOther>& rhs) const noexcept
	{
		return allocatorID != rhs.allocatorID;
	}

	[[nodiscard]] auto GetID() const
	{
		return allocatorID;
	}

	[[nodiscard]] auto GetSourceAllocatorID() const
	{
		return allocatorID;
	}

	[[nodiscard]] static constexpr size_t GetFallbackCount()
	{
		return 0;
	}
};
} // namespace hbe

#ifdef __TEST__
#include "Test/TestCollection.h"

namespace hbe
{
class BaseAllocatorTest : public TestCollection
{
public:
	BaseAllocatorTest()
		: TestCollection("BaseAllocatorTest")
	{
	}

protected:
	void Prepare() noexcept override;
};
} // namespace hbe
#endif //__TEST__
