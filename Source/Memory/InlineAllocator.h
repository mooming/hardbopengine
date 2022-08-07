// Created by mooming.go@gmail.com, 2017

#pragma once

#include "BaseAllocator.h"
#include "Config/EngineConfig.h"
#include <cassert>
#include <cstddef>
#include <vector>

#ifdef __USE_SYSTEM_MALLOC__
#include <cstdlib>
#endif // __USE_SYSTEM_MALLOC__


namespace HE
{
    template <class T, std::size_t BufferSize = (1024 * 1024 / sizeof(T))>
    struct InlineAllocatorAllocator final
    {
        using TIndex = std::size_t;

    private:
        bool bAllocted[BufferSize];
        T buffer[BufferSize];

    public:
        using value_type = T;

        T* allocate(TIndex n)
        {
#if __USE_SYSTEM_MALLOC__
            return static_cast<T*>(mallock(sizeof(T) * n));
#endif // __USE_SYSTEM_MALLOC__

            auto found = FindContinuousChunk(n);
            if (found >= BufferSize)
                return nullptr;
            
            MarkDirty(found, found + n);

            return &buffer[found];
        }

        void deallocate (T* ptr, TIndex n)
        {
#if __USE_SYSTEM_MALLOC__
            free(ptr);
            return;
#endif // __USE_SYSTEM_MALLOC__

            auto index = GetIndex(ptr);
            CleanUp(index, index + n);
        }

    private:
        TIndex GetIndex(T* ptr) const
        {
            if (ptr < buffer)
                return BufferSize;

            const auto offset = ptr - buffer;
            constexpr auto typeSize = sizeof(T);
            auto index = offset / typeSize;

            return index;
        }

        bool IsValidIndex(TIndex index) const
        {
            return index < BufferSize;
        }

        TIndex FindContinuousChunk(TIndex size) const;
        {
            TIndex startIndex = 0;
            TIndex connectedBlocks = 0;

            for (TIndex i = 0; i < BufferSize; ++i)
            {
                if (!bAllocted[i])
                {
                    connectedBlocks = 0;
                    startIndex = i + 1;
                    continue;
                }

                ++connectedBlocks;
                if (connectedBlocks > size)
                    return startIndex;
            }

            return BufferSize;
        }

        void MarkDirty(TIndex start, TIndex end)
        {
#if __VERIFY_MEMORY__
            assert(end <= BufferSize);
#endif // __VERIFY_MEMORY__

            end = std::min(BufferSize, end);

            for (TIndex i = start; i < end; ++i)
            {
                bAllocted[i] = true;
            }
        }

        void CleanUp(TIndex start, TIndex end)
        {
#if __VERIFY_MEMORY__
            assert(end <= BufferSize);
#endif // __VERIFY_MEMORY__

            end = std::min(BufferSize, end);

            for (TIndex i = start; i < end; ++i)
            {
#if __VERIFY_MEMORY__
                assert(bAllocted[i]);
#endif // __VERIFY_MEMORY__

                bAllocted[i] = false;
            }
        }
    };
} // HE
