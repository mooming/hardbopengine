// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "Core/Debug.h"
#include "Engine/Engine.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"

namespace hbe
{
	/// @brief Allocator that uses the system heap (malloc/free).
	/// @details Wraps OS-level memory allocation functions.
	template<class T>
	class SystemAllocator
	{
	public:
		using value_type = T;

		template<class U>
		struct rebind
		{
			using other = SystemAllocator<U>;
		};

		SystemAllocator() = default;
		~SystemAllocator() = default;

#ifdef _MSC_VER
		template<class U>
		SystemAllocator(const SystemAllocator<U>&)
		{}
#endif // _MSC_VER

		[[nodiscard]] constexpr auto GetID() const { return MemoryManager::SystemAllocatorID; }

		[[nodiscard]] T* allocate(std::size_t n) noexcept { return reinterpret_cast<T*>(AllocateBytes(n * sizeof(T))); }

		void deallocate(T* ptr, std::size_t n) noexcept {
			Assert(ptr != nullptr);
			DeallocateBytes(ptr, n * sizeof(T));
		}

		template<class U>
		bool operator==(const SystemAllocator<U>&) noexcept
		{
			return true;
		}

		template<class U>
		bool operator!=(const SystemAllocator<U>&) noexcept
		{
			return false;
		}

		[[nodiscard]] auto GetName() const { return "SystemAllocator"; }

	private:
		void* AllocateBytes(std::size_t nBytes)
		{
#if MEMORY_INVESTIGATION_ENABLED
			if (unlikely(nBytes == 1))
			{
				nBytes = 1;
			}

			const auto pageSize = OS::GetPageSize();
			const size_t pageCount = (nBytes + pageSize - 1) / pageSize;
			const size_t allocSize = pageCount * pageSize;
			auto rawPtr = OS::VirtualAlloc(allocSize);

#if MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED
			T* ptr = reinterpret_cast<T*>(rawPtr);
#else // MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED
			auto bytePtr = static_cast<uint8_t*>(rawPtr);
			bytePtr = bytePtr + allocSize - nBytes;
			T* ptr = reinterpret_cast<T*>(bytePtr);
#endif // MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED

#if MEMORY_LOGGING_ENABLED
			{
				auto& engine = Engine::Get();
				engine.Log(ELogLevel::Info, [this, rawPtr, ptr](auto& ls)
				{ ls << '[' << GetName() << "][Investigator] RawPtr = " << rawPtr << ", ptr = " << (void*) ptr; });
			}
#endif // MEMORY_LOGGING_ENABLED

#else // MEMORY_INVESTIGATION_ENABLED
			auto ptr = (T*) malloc(nBytes);
#endif // MEMORY_INVESTIGATION_ENABLED

#if PROFILE_ENABLED
			auto& mmgr = MemoryManager::GetInstance();

#if MEMORY_INVESTIGATION_ENABLED
			const auto allocated = allocSize;
#else // MEMORY_INVESTIGATION_ENABLED
			const auto allocated = OS::GetAllocSize(ptr);
#endif // MEMORY_INVESTIGATION_ENABLED

			mmgr.ReportAllocation(GetID(), ptr, nBytes, allocated);
#endif // PROFILE_ENABLED

			return ptr;
		}

		void DeallocateBytes(void* ptr, std::size_t nBytes)
		{
			Assert(ptr != nullptr, "[SysAlloc][Dealloc] Null Pointer Error");

#if MEMORY_INVESTIGATION_ENABLED
			const auto pageSize = OS::GetPageSize();
			const size_t pageCount = (nBytes + pageSize - 1) / pageSize;
			const size_t allocSize = pageCount * pageSize;

#if MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED
			auto rawPtr = ptr;
#else // MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED
			auto bytePtr = reinterpret_cast<uint8_t*>(ptr);
			bytePtr = bytePtr + nBytes - allocSize;
			auto rawPtr = (void*) bytePtr;
#endif // MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED
			const auto allocated = allocSize;
#endif // MEMORY_INVESTIGATION_ENABLED

#if PROFILE_ENABLED

#ifndef MEMORY_INVESTIGATION_ENABLED
			auto allocated = OS::GetAllocSize(ptr);
#else
			const std::size_t allocated = nBytes;
#endif // MEMORY_INVESTIGATION_ENABLED

#if MEMORY_VERIFICATION_ENABLED
			Assert(nBytes <= allocated);
#endif // MEMORY_VERIFICATION_ENABLED

			auto& mmgr = MemoryManager::GetInstance();
			mmgr.ReportDeallocation(GetID(), ptr, nBytes, allocated);
#endif // PROFILE_ENABLED

#if MEMORY_INVESTIGATION_ENABLED

#if MEMORY_DANGLING_POINTER_CHECK_ENABLED
			OS::ProtectMemory(rawPtr, allocated);
#else // MEMORY_DANGLING_POINTER_CHECK_ENABLED
			OS::VirtualFree(rawPtr, allocated);
#endif // MEMORY_DANGLING_POINTER_CHECK_ENABLED

#else // MEMORY_INVESTIGATION_ENABLED
			free(ptr);
#endif // MEMORY_INVESTIGATION_ENABLED
		}
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class SystemAllocatorTest : public TestCollection
	{
	public:
		SystemAllocatorTest() : TestCollection("SystemAllocatorTest") {}

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
