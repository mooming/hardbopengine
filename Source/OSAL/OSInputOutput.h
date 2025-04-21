// Created by mooming.go@gmail.com 2022

#pragma once

#include "String/StaticString.h"

namespace OS
{

struct FileHandle;
struct FileOpenMode;
struct ProtectionMode;
struct MapSyncMode;

bool Open(
    FileHandle& outHandle, HE::StaticString filePath, FileOpenMode openMode);
bool Close(FileHandle&& handle);
bool Exist(HE::StaticString filePath);
bool Delete(HE::StaticString filePath);

size_t Read(const FileHandle& handle, void* buffer, size_t size);
size_t Write(const FileHandle& handle, void* buffer, size_t size);
bool Truncate(const FileHandle& handle, size_t size);

void* MapMemory(FileHandle& fileHandle, size_t size, ProtectionMode protection,
    size_t offset);
bool MapSync(void* ptr, size_t size, MapSyncMode syncMode);
bool UnmapMemory(void* ptr, size_t size);

} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class OSInputOutputTest : public TestCollection
{
public:
    OSInputOutputTest();

protected:
    virtual void Prepare() override;
};

} // namespace HE
#endif //__UNIT_TEST__
