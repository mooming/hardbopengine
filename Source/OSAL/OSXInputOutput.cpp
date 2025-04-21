// Created by mooming.go@gmail.com 2022

#include "OSInputOutput.h"

#include "Log/Logger.h"
#include "OSFileHandle.h"
#include "OSFileOpenMode.h"
#include "OSMapSyncMode.h"
#include "OSProtectionMode.h"
#include "String/StringUtil.h"

#ifdef PLATFORM_OSX
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace OS
{

namespace FileHandleHelper
{

int GetHandle(const FileHandle& handle)
{
    auto& fd = reinterpret_cast<const int&>(handle.data);
    return fd;
}

void SetHandle(FileHandle& outHandle, int fd)
{
    int& data = reinterpret_cast<int&>(outHandle.data);
    data = fd;
}

} // namespace FileHandleHelper

bool Open(
    FileHandle& outHandle, HE::StaticString filePath, FileOpenMode openMode)
{
    auto path = filePath.c_str();
    int fd = ::open(path, openMode.value, S_IRUSR | S_IWUSR);
    if (unlikely(fd < 0))
    {
        using namespace HE;
        using namespace StringUtil;
        static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

        log.OutWarning([&filePath](auto& ls) {
            ls << "File open failed. path = " << filePath << ", reason("
               << std::strerror(errno) << ')';
        });

        return false;
    }

    FileHandleHelper::SetHandle(outHandle, fd);

    return true;
}

bool Close(FileHandle&& inHandle)
{
    using namespace HE;
    using namespace StringUtil;
    using namespace FileHandleHelper;

    static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

    auto handle = std::move(inHandle);

    int fd = GetHandle(handle);
    FileHandleHelper::SetHandle(handle, -1);

    if (unlikely(fd < 0))
    {
        log.OutWarning(
            [fd](auto& ls) { ls << "File Close (fd:" << fd << ") failed."; });

        return false;
    }

    auto result = close(fd);
    if (unlikely(result < 0))
    {
        log.OutWarning([fd, result](auto& ls) {
            ls << "File Close (fd:" << fd << ") failed. result = " << result
               << ", reason(" << std::strerror(errno) << ')';
        });

        return false;
    }

    return true;
}

bool Exist(HE::StaticString filePath)
{
    auto rc = access(filePath.c_str(), F_OK);
    return rc == 0;
}

bool Delete(HE::StaticString filePath)
{
    auto rc = remove(filePath.c_str());
    return rc == 0;
}

size_t Read(const FileHandle& handle, void* buffer, size_t size)
{
    using namespace HE;
    using namespace StringUtil;
    using namespace FileHandleHelper;

    static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

    int fd = GetHandle(handle);
    if (unlikely(fd < 0))
    {
        log.OutWarning(
            [fd](auto& ls) { ls << "Invalid file handle (fd:" << fd << ")."; });

        return 0;
    }

    if (unlikely(buffer == nullptr))
    {
        log.OutWarning([](auto& ls) { ls << "Null buffer error."; });

        return 0;
    }

    if (unlikely(size == 0))
    {
        log.OutWarning([](auto& ls) { ls << "Zero size error."; });

        return 0;
    }

    auto result = read(fd, buffer, size);
    if (unlikely(result != size))
    {
        log.OutWarning([size, result](auto& ls) {
            ls << "Read failed. Read done " << result << ", but " << size
               << " is expected. Reason(" << std::strerror(errno) << ')';
        });

        return result;
    }

    return result;
}

size_t Write(const FileHandle& handle, void* buffer, size_t size)
{
    using namespace HE;
    using namespace StringUtil;
    using namespace FileHandleHelper;

    static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

    int fd = GetHandle(handle);
    if (unlikely(fd < 0))
    {
        log.OutWarning(
            [fd](auto& ls) { ls << "Invalid file handle (fd:" << fd << ")."; });

        return 0;
    }

    if (unlikely(buffer == nullptr))
    {
        log.OutWarning([](auto& ls) { ls << "Null buffer error."; });

        return 0;
    }

    if (unlikely(size == 0))
    {
        log.OutWarning([](auto& ls) { ls << "Zero size error."; });

        return 0;
    }

    auto result = write(fd, buffer, size);
    if (unlikely(result != size))
    {
        log.OutWarning([size, result](auto& ls) {
            ls << "Write failed. Written " << result << ", but " << size
               << " is expected. Reason(" << std::strerror(errno) << ')';
        });

        return result;
    }

    return result;
}

bool Truncate(const FileHandle& handle, size_t size)
{
    using namespace HE;
    using namespace StringUtil;
    using namespace FileHandleHelper;

    static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

    int fd = GetHandle(handle);
    if (unlikely(fd < 0))
    {
        log.OutWarning(
            [fd](auto& ls) { ls << "Invalid file handle (fd:" << fd << ")."; });

        return 0;
    }

    auto result = ftruncate(fd, size);
    if (unlikely(result != 0))
    {
        log.OutWarning([fd, size](auto& ls) {
            ls << "Failed to truncate the file(" << fd << ") with the size "
               << size << ". Reason(" << std::strerror(errno) << ')';
        });

        return 0;
    }

    return true;
}

void* MapMemory(FileHandle& fileHandle, size_t size, ProtectionMode protection,
    size_t offset)
{
    using namespace HE;
    using namespace StringUtil;

    static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

    int fd = FileHandleHelper::GetHandle(fileHandle);
    if (unlikely(fd < 0))
    {
        log.OutWarning(
            [fd](auto& ls) { ls << "Invalid file handle. fd = " << fd; });

        return nullptr;
    }

    if (unlikely(size == 0))
    {
        log.OutWarning([size](auto& ls) { ls << "Invalid size = " << size; });

        return nullptr;
    }

    auto ptr = mmap(nullptr, size, protection.value, MAP_SHARED, fd, offset);
    if (unlikely(ptr == MAP_FAILED))
    {
        log.OutError([fd, size, protection, offset](auto& ls) {
            ls << "Failed to map the file(" << fd
               << ") to a memory address. ErrorMsg(" << std::strerror(errno)
               << ") with arguments size = " << size
               << ", protection = " << protection.value
               << ", offset = " << offset;
        });

        return nullptr;
    }

    return ptr;
}

bool MapSync(void* ptr, size_t size, MapSyncMode syncMode)
{
    if (unlikely(ptr == nullptr))
    {
        return false;
    }

    if (unlikely(size == 0))
    {
        return true;
    }

    auto rc = msync(ptr, size, syncMode.value);
    if (unlikely(rc != 0))
    {
        using namespace HE;
        using namespace StringUtil;
        static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));
        log.OutError([ptr](auto& ls) {
            ls << "Failed to set sync. ptr = " << ptr << ", ErrorMsg("
               << std::strerror(errno) << ')';
        });
    }

    return true;
}

bool UnmapMemory(void* ptr, size_t size)
{
    using namespace HE;
    using namespace StringUtil;

    static auto log = Logger::Get(ToFunctionName(__PRETTY_FUNCTION__));

    if (unlikely(ptr == nullptr))
    {
        log.OutWarning([](auto& ls) { ls << "Null pointer input."; });

        return false;
    }

    auto result = munmap(ptr, size);
    if (unlikely(result != 0))
    {
        log.OutError([ptr, size](auto& ls) {
            ls << "Failed to unmap(" << ptr << ") with the size " << size
               << ". ErrorMsg(" << std::strerror(errno) << ')';
        });
    }

    return true;
}

} // namespace OS
#endif // PLATFORM_OSX
