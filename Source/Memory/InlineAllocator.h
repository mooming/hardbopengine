// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"
#include "BaseAllocator.h"
#include "MemoryManager.h"
#include "Config/EngineConfig.h"
#include "OSAL/OSMemory.h"
#include "System/Debug.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <thread>
#include <type_traits>

#ifdef __USE_SYSTEM_MALLOC__
#include <cstdlib>
#endif // __USE_SYSTEM_MALLOC__


namespace HE
{
    template <class T, int BufferSize = 64, class FallbackAllocator = BaseAllocator<T>>
    struct ALIGN_STRUCT InlineAllocator final
    {
        using value_type = T;
        using TIndex = decltype(BufferSize);
        
        static_assert(std::is_signed<TIndex>(), "The type of BufferSize should be signed integral.");
        static_assert(BufferSize >= 0, "The buffer size should be greater than or equal to zero.");
        
    private:
        TAllocatorID id;
        std::thread::id threadId;
        TIndex sizeOfBlock[BufferSize];
        T buffer[BufferSize];
        FallbackAllocator fallbackAllocator;

    public:
        InlineAllocator()
            : id(InvalidAllocatorID)
        {
#ifdef __MEMORY_VERIFICATION__
            Assert(OS::CheckAligned(this));
            Assert(OS::CheckAligned(&sizeOfBlock[0]));
            Assert(OS::CheckAligned(&buffer[0]));
#endif // __MEMORY_VERIFICATION__

#ifdef __MEMORY_LOGGING__
            std::cout << "[InlineAllocator][Stat][" << (void*)this
                << "] Created: " << BufferSize << ", "
                << (sizeof(T) * BufferSize) << " bytes" << std::endl;
#endif // __MEMORY_LOGGING__
            
            RegisterAllocator();
            
            if (unlikely(BufferSize <= 0))
                return;
            
            sizeOfBlock[0] = BufferSize;
           
#ifdef __MEMORY_VERIFICATION__
            memset(sizeOfBlock + 1, 0, sizeof(TIndex) * BufferSize);
            memset(buffer, 0, sizeof(T) * BufferSize);
#endif // __MEMORY_VERIFICATION__
        }
        
        ~InlineAllocator()
        {
            DeregisterAllocator();
        }

        T* allocate(std::size_t n)
        {
            auto FallbackAlloc = [this, n]() -> T*
            {
                auto ptr = fallbackAllocator.allocate(n);
                return ptr;
            };
            
            if (n > BufferSize)
                return FallbackAlloc();
            
            if (unlikely(n == 0))
                return &buffer[0];

#ifdef __MEMORY_VERIFICATION__
            Assert(BufferSize > 0);
#endif // __MEMORY_VERIFICATION__
            
            TIndex numSize = static_cast<TIndex>(n);
            auto found = FindContinuousChunk(numSize);
            if (!IsValidIndex(found))
                return FallbackAlloc();

            AllocBlock(found, numSize);
            
            auto ptr = &buffer[found];
            
#ifdef __MEMOR_STATISTICS__
            auto& mmgr = MemoryManager::GetInstance();
            const auto nBytes = n * sizeof(T);
            mmgr.ReportAllocation(id, ptr, nBytes, nBytes);
#endif // __MEMOR_STATISTICS__
            
            return ptr;
        }

        void deallocate (T* ptr, std::size_t n) noexcept
        {
            if (unlikely(ptr == nullptr || n == 0))
                return;
            
            auto index = GetIndex(ptr);
            if (!IsValidIndex(index))
            {
                fallbackAllocator.deallocate(ptr, n);
                return;
            }
            
#ifdef __MEMOR_STATISTICS__
            auto& mmgr = MemoryManager::GetInstance();
            const auto nBytes = n * sizeof(T);
            mmgr.ReportDeallocation(id, ptr, nBytes, nBytes);
#endif // __MEMOR_STATISTICS__

            ReleaseBlock(index, n);
        }

    private:
        TIndex GetIndex(T* ptr) const
        {
            const T* bufferPtr = buffer;
            if (ptr < bufferPtr)
                return BufferSize;

            TIndex index = ptr - bufferPtr;

            return index;
        }

        bool IsValidIndex(TIndex index) const
        {
            return index > 0 && index < BufferSize;
        }

        TIndex FindContinuousChunk(TIndex size)
        {
            for (TIndex i = 0; i < BufferSize;)
            {
                auto blockSize = sizeOfBlock[i];
                
#ifdef __MEMORY_VERIFICATION__
                Assert(blockSize != 0);
#endif // __MEMORY_VERIFICATION__
                
                if (blockSize < 0)
                {
                    i -= blockSize;
                    continue;
                }
                
                TIndex freeSize = blockSize;
                
                for (TIndex j = i + freeSize; j < BufferSize;)
                {
                    auto tmpSize = sizeOfBlock[j];
                    
#ifdef __MEMORY_VERIFICATION__
                    Assert(tmpSize != 0);
#endif // __MEMORY_VERIFICATION__
                    if (tmpSize < 0)
                        break;
                    
                    freeSize += tmpSize;
                    
#ifdef __MEMORY_VERIFICATION__
                    sizeOfBlock[j] = 0;
#endif // __MEMORY_VERIFICATION__
                    
                    j += tmpSize;
                }
                
                sizeOfBlock[i] = freeSize;
                
                if (freeSize < size)
                {
                    i += freeSize;
                    continue;
                }
                
                return i;
            }
            
            return BufferSize;
        }

        void AllocBlock(TIndex start, TIndex size)
        {
            auto index = start;
            auto blockSize = sizeOfBlock[index];
            auto remained = blockSize - size;

#ifdef __MEMORY_VERIFICATION__
            Assert(size <= std::numeric_limits<TIndex>::max());
            Assert(remained >= 0);
#endif // __MEMORY_VERIFICATION__

            sizeOfBlock[index] = -static_cast<TIndex>(size);
            
            if (remained <= 0)
                return;
            
            index += size;
            if (index >= BufferSize)
                return;
            
            sizeOfBlock[index] = remained;
        }

        void ReleaseBlock(TIndex start, TIndex size)
        {
#ifdef __MEMORY_VERIFICATION__
            Assert(size > 0);
            Assert(-sizeOfBlock[start] == size);
#endif // __MEMORY_VERIFICATION__
            
            sizeOfBlock[start] = size;
            
            for (TIndex i = start + size; i < BufferSize;)
            {
                auto blockSize = sizeOfBlock[i];
                if (blockSize <= 0)
                    break;
                
#ifdef __MEMORY_VERIFICATION__
                sizeOfBlock[i] = 0;
#endif // __MEMORY_VERIFICATION__
                
                sizeOfBlock[start] += blockSize;
                i += blockSize;
            }
        }
        
    private:
        void RegisterAllocator()
        {
            auto& mmgr = MemoryManager::GetInstance();
            
            auto allocFunc = [this](size_t nBytes) -> void*
            {
                constexpr auto unit = sizeof(T);
                if (unit == 1)
                {
                    return static_cast<void*>(allocate(nBytes));
                }
                else
                {
                    const auto unitCounts = (nBytes + unit - 1) / unit;
                    return static_cast<void*>(allocate(unitCounts));
                }
            };
            
            auto deallocFunc = [this](void* ptr, size_t nBytes)
            {
                constexpr auto unit = sizeof(T);
                if (unit == 1)
                {
                    deallocate(static_cast<T*>(ptr), nBytes);
                    return;
                }
                else
                {
                    const auto unitCounts = (nBytes + unit - 1) / unit;
                    deallocate(static_cast<T*>(ptr), unitCounts);
                }
            };
            
            id = mmgr.Register("InlineAllocator", true
                , BufferSize * sizeof(T), allocFunc, deallocFunc);
        }
        
        void DeregisterAllocator()
        {
            auto& mmgr = MemoryManager::GetInstance();
            mmgr.Deregister(id);
        }
    };
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCase.h"

namespace HE
{
	class InlineAllocatorTest : public TestCase
	{
	public:

		InlineAllocatorTest()
            : TestCase("InlineAllocatorTest")
		{
		}

	protected:
		virtual bool DoTest() override;
	};
} // HE
#endif //__UNIT_TEST__
