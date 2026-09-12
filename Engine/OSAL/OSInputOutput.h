// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "String/StaticString.h"

namespace OS
{

class FileHandle;
class FileOpenMode;
class ProtectionMode;
class MapSyncMode;

bool Open(FileHandle& outHandle, hbe::StaticString filePath, FileOpenMode openMode) noexcept;
bool Close(FileHandle&& handle) noexcept;
bool Exist(hbe::StaticString filePath) noexcept;
bool Delete(hbe::StaticString filePath) noexcept;

size_t Read(const FileHandle& handle, void* buffer, size_t size) noexcept;
size_t Write(const FileHandle& handle, void* buffer, size_t size) noexcept;
bool Truncate(const FileHandle& handle, size_t size) noexcept;

void* MapMemory(FileHandle& fileHandle, size_t size, ProtectionMode protection, size_t offset) noexcept;
bool MapSync(void* ptr, size_t size, MapSyncMode syncMode) noexcept;
bool UnmapMemory(void* ptr, size_t size) noexcept;

} // namespace OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

/// @brief Test collection for OS input/output operations.
class OSInputOutputTest final : public TestCollection
{
public:
	OSInputOutputTest() noexcept;

protected:
	void Prepare() override;
};

} // namespace hbe
#endif //__UNIT_TEST__
