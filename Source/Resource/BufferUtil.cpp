// Created by mooming.go@gmail.com, 2022

#include "BufferUtil.h"

#include "Log/Logger.h"
#include "OSAL/OSFileHandle.h"
#include "OSAL/OSInputOutput.h"
#include "OSAL/OSMapSyncMode.h"
#include "String/StringUtil.h"

namespace HE
{

    namespace BufferUtil
    {

        Buffer GenerateDummyBuffer(size_t size)
        {
            auto generator = [size](TSize &outSize, TBufferData &outData) {
                outSize = size;
                outData = nullptr;
            };

            return Buffer(generator);
        }

        Buffer GenerateFileBuffer(StaticString path, OS::FileOpenMode openMode,
            OS::ProtectionMode protection, size_t size)
        {
            using namespace OS;
            using namespace StringUtil;

            static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

            FileHandle fh;

            auto generator = [&fh, path, openMode, protection, size](
                                 TSize &outSize, TBufferData &outData) {
                outSize = 0;
                outData = nullptr;

                if (!Open(fh, path, openMode))
                {
                    log.OutError(
                        [path](auto &ls) { ls << "Failed to open " << path; });

                    return;
                }

                auto fileSize = fh.GetFileSize();
                if (size != 0)
                {
                    fileSize = size;
                    if (!Truncate(fh, fileSize))
                    {
                        log.OutWarning([path, size](auto &ls) {
                            ls << "Failed to resize the file " << path
                               << " to the given size " << size;
                        });

                        return;
                    }
                }

                if (fileSize <= 0)
                {
                    log.OutWarning([path](auto &ls) {
                        ls << "Nothing to map. File size is zero. path = "
                           << path;
                    });

                    return;
                }

                auto ptr = MapMemory(fh, fileSize, protection, 0);
                if (unlikely(ptr == nullptr))
                {
                    log.OutError(
                        [path](auto &ls) { ls << "Failed to map " << path; });

                    return;
                }

                outSize = fileSize;
                outData = reinterpret_cast<decltype(outData)>(ptr);
            };

            Buffer buffer(generator);
            if (buffer.GetData() == nullptr)
            {
                return buffer;
            }

            auto handleData = fh.data;
            fh.Invalidate();

            buffer.SetReleaser(
                [handleData](TSize size, TBufferData data) mutable {
                    if (unlikely(data == nullptr))
                    {
                        return;
                    }

                    FileHandle fh;
                    fh.data = handleData;

                    Assert(size > 0);
                    auto ptr = static_cast<void *>(data);

                    MapSyncMode syncMode;
                    syncMode.SetSync();

                    MapSync(ptr, size, syncMode);
                    UnmapMemory((void *)data, size);

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
            openMode.SetReadWrite();

            ProtectionMode protection;
            protection.SetReadable();

            return GenerateFileBuffer(path, openMode, protection);
        }

        Buffer GetWriteOnlyFileBuffer(StaticString path, size_t size)
        {
            using namespace OS;

            FileOpenMode openMode;
            openMode.SetReadWrite();

            if (Exist(path))
            {
                openMode.SetTruncate();
            }
            else
            {
                openMode.SetCreate();
            }

            ProtectionMode protection;
            protection.SetWritable();

            return GenerateFileBuffer(path, openMode, protection, size);
        }

    } // namespace BufferUtil

} // namespace HE
