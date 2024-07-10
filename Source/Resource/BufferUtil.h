// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Buffer.h"
#include "BufferTypes.h"
#include "Memory/MemoryManager.h"
#include "OSAL/OSFileOpenMode.h"
#include "OSAL/OSProtectionMode.h"
#include "String/StaticString.h"
#include "System/Debug.h"
#include <limits>


namespace OS
{
struct FileHandle;
};

namespace HE
{
namespace BufferUtil
{
using namespace BufferTypes;

Buffer GenerateDummyBuffer(size_t size = std::numeric_limits<size_t>::max());
Buffer GenerateFileBuffer(
    StaticString path, OS::FileOpenMode openMode, OS::ProtectionMode protection, size_t size = 0);
Buffer GetFileBuffer(StaticString path);
Buffer GetReadOnlyFileBuffer(StaticString path);
Buffer GetWriteOnlyFileBuffer(StaticString path, size_t size);

template <typename T>
Buffer GetMemoryBuffer(TSize numElements, const T& defaultValue)
{
    auto& mmgr = MemoryManager::GetInstance();
    auto generator = [&mmgr, numElements, defaultValue](TSize& outSize, TBufferData& outData)
    {
        outSize = numElements * sizeof(T);
        auto ptr = mmgr.NewArray<T>(numElements, defaultValue);
        outData = reinterpret_cast<TBufferData>(ptr);
    };

    auto releaser = [&mmgr](TSize size, TBufferData data)
    {
        const TSize numElements = size / sizeof(T);
        Assert((numElements * sizeof(T)) == size);

        auto ptr = reinterpret_cast<T*>(data);
        mmgr.DeleteArray<T>(ptr, numElements);
    };

    return Buffer(generator, releaser);
}

} // namespace BufferUtil
} // namespace HE
