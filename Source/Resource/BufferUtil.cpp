// Created by mooming.go@gmail.com, 2022

#include "BufferUtil.h"

#include "Log/Logger.h"
#include "OSAL/OSFileHandle.h"
#include "OSAL/OSInputOutput.h"
#include "String/StringUtil.h"


namespace HE
{

namespace BufferUtil
{

Buffer GenerateFileBuffer(StaticString path, OS::FileOpenMode openMode
    , OS::ProtectionMode protection)
{
    using namespace OS;
    using namespace StringUtil;

    static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

    FileHandle fh;

    auto generator = [&fh, path, openMode, protection](TSize& outSize, TBufferData& outData)
    {
        outSize = 0;
        outData = nullptr;

        if (!Open(fh, path, openMode))
        {
            log.OutError([path](auto& ls)
            {
                ls << "Failed to open " << path;
            });

            return;
        }

        const auto fileSize = fh.GetFileSize();
        if (fileSize <= 0)
        {
            log.OutWarning([path](auto& ls)
            {
                ls << "Nothing to map. File size is zero. path = " << path;
            });

            return;
        }

        auto ptr = MapMemory(fh, fileSize, protection, 0);
        if (unlikely(ptr == nullptr))
        {
            log.OutError([path](auto& ls)
            {
                ls << "Failed to map " << path;
            });

            return;
        }

        outSize = fileSize;
        outData = reinterpret_cast<decltype(outData)>(ptr);
    };

    Buffer buffer(generator);

    auto handleData = fh.data;
    fh.Invalidate();

    buffer.SetReleaser([handleData](TSize size, TBufferData data) mutable
    {
        if (data == nullptr)
            return;

        FileHandle fh;
        fh.data = handleData;

        Assert(size > 0);
        UnmapMemory((void*)data, size);

        OS::Close(std::move(fh));
    });

    return buffer;
}

Buffer GetFileBuffer(StaticString path)
{
    using namespace OS;

    FileOpenMode openMode;
    openMode.SetReadWrite();

    ProtectionMode protection;
    protection.SetReadable();
    protection.SetWritable();

    return GenerateFileBuffer(path, openMode, protection);
}

Buffer GetReadOnlyFileBuffer(StaticString path)
{
    using namespace OS;

    FileOpenMode openMode;
    openMode.SetReadOnly();

    ProtectionMode protection;
    protection.SetReadable();

    return GenerateFileBuffer(path, openMode, protection);
}

Buffer GetWriteOnlyFileBuffer(StaticString path)
{
    using namespace OS;

    FileOpenMode openMode;
    openMode.SetWriteOnly();

    ProtectionMode protection;
    protection.SetWritable();

    return GenerateFileBuffer(path, openMode, protection);
}

} // BufferUtil

} // HE
