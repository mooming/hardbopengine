// Created by mooming.go@gmail.com 2022

#include "OSFileHandle.h"

#include "Config/BuildConfig.h"
#include "Intrinsic.h"
#include "OSInputOutput.h"


#ifdef PLATFORM_OSX
#include <sys/stat.h>


namespace OS
{

namespace
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

} // namespace

FileHandle::FileHandle()
{
    Invalidate();
}

FileHandle::FileHandle(FileHandle&& rhs) : data(rhs.data)
{
    rhs.Invalidate();
}

FileHandle::~FileHandle()
{
    auto fd = GetHandle(*this);
    if (fd < 0)
    {
        return;
    }

    Close(std::move(*this));
}

size_t FileHandle::GetFileSize() const
{
    auto fd = GetHandle(*this);
    if (unlikely(fd < 0))
    {
        return 0;
    }

    struct stat statValue;
    int result = fstat(fd, &statValue);

    if (unlikely(result != 0))
    {
        return 0;
    }

    static_assert(sizeof(size_t) == sizeof(statValue.st_size));

    return statValue.st_size;
}

bool FileHandle::IsValid() const
{
    auto fd = GetHandle(*this);
    return fd >= 0;
}

void FileHandle::Invalidate()
{
    SetHandle(*this, -1);
}

} // namespace OS
#endif // PLATFORM_OSX
