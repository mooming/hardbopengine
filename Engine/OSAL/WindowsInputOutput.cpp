// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSInputOutput.h"

#include <fcntl.h>
#include <windows.h>

#include "Core/CommonMacros.h"
#include "Log/Logger.h"
#include "OSFileHandle.h"
#include "OSFileOpenMode.h"
#include "OSMapSyncMode.h"
#include "OSProtectionMode.h"
#include "String/StringUtil.h"
#include "WindowsIntrinsic.h"

#if PLATFORM_WINDOWS

namespace OS
{
bool Open(FileHandle& outHandle, hbe::StaticString filePath, FileOpenMode openMode) noexcept
{
	using namespace hbe;
	using namespace StringUtil;
	static auto log = Logger::Get(ToFunctionName(__FUNCSIG__));

	const int mode = openMode.value;

	DWORD access = GENERIC_READ;
	if ((mode & O_RDWR) != 0)
		access = GENERIC_READ | GENERIC_WRITE;
	else if ((mode & O_WRONLY) != 0)
		access = GENERIC_WRITE;

	if ((mode & O_APPEND) != 0)
		access |= FILE_APPEND_DATA;

	DWORD disposition = OPEN_EXISTING;
	if ((mode & O_CREAT) != 0)
		disposition = ((mode & O_TRUNC) != 0) ? CREATE_ALWAYS : OPEN_ALWAYS;
	else if ((mode & O_TRUNC) != 0)
		disposition = TRUNCATE_EXISTING;

	HANDLE handle = CreateFileA(filePath.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, disposition,
		FILE_ATTRIBUTE_NORMAL, nullptr);

	if (unlikely(handle == INVALID_HANDLE_VALUE))
	{
		log.OutWarning([&filePath](auto& ls)
		{ ls << "File open failed. path = " << filePath << ", reason(" << GetLastError() << ')'; });

		return false;
	}

	outHandle.data = handle;

	return true;
}

bool Close(FileHandle&& inHandle) noexcept
{
	using namespace hbe;
	using namespace StringUtil;
	static auto log = Logger::Get(ToFunctionName(__FUNCSIG__));

	auto handle = std::move(inHandle);
	HANDLE fileHandle = static_cast<HANDLE>(handle.data);
	handle.Invalidate();

	if (unlikely(fileHandle == nullptr))
	{
		log.OutWarning([](auto& ls) { ls << "File Close failed. The handle is null."; });
		return false;
	}

	if (unlikely(CloseHandle(fileHandle) == FALSE))
	{
		log.OutWarning([fileHandle](auto& ls)
		{ ls << "File Close failed. handle = " << fileHandle << ", reason(" << GetLastError() << ')'; });

		return false;
	}

	return true;
}

bool Exist(hbe::StaticString filePath) noexcept
{
	return GetFileAttributesA(filePath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool Delete(hbe::StaticString filePath) noexcept
{
	return DeleteFileA(filePath.c_str()) != FALSE;
}

size_t Read(const FileHandle& handle, void* buffer, size_t size) noexcept
{
	using namespace hbe;
	using namespace StringUtil;
	static auto log = Logger::Get(ToFunctionName(__FUNCSIG__));

	if (unlikely(!handle.IsValid()))
	{
		log.OutWarning([](auto& ls) { ls << "Invalid file handle."; });
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

	DWORD readBytes = 0;
	BOOL result = ReadFile(static_cast<HANDLE>(handle.data), buffer, static_cast<DWORD>(size), &readBytes, nullptr);

	if (unlikely(result == FALSE))
	{
		log.OutWarning([size](auto& ls)
		{ ls << "Read failed. " << size << " is expected. Reason(" << GetLastError() << ')'; });

		return 0;
	}

	return readBytes;
}

size_t Write(const FileHandle& handle, void* buffer, size_t size) noexcept
{
	using namespace hbe;
	using namespace StringUtil;
	static auto log = Logger::Get(ToFunctionName(__FUNCSIG__));

	if (unlikely(!handle.IsValid()))
	{
		log.OutWarning([](auto& ls) { ls << "Invalid file handle."; });
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

	DWORD writtenBytes = 0;
	BOOL result = WriteFile(static_cast<HANDLE>(handle.data), buffer, static_cast<DWORD>(size), &writtenBytes, nullptr);

	if (unlikely(result == FALSE))
	{
		log.OutWarning([size](auto& ls)
		{ ls << "Write failed. " << size << " is expected. Reason(" << GetLastError() << ')'; });

		return 0;
	}

	return writtenBytes;
}

bool Truncate(const FileHandle& handle, size_t size) noexcept
{
	using namespace hbe;
	using namespace StringUtil;
	static auto log = Logger::Get(ToFunctionName(__FUNCSIG__));

	if (unlikely(!handle.IsValid()))
	{
		log.OutWarning([](auto& ls) { ls << "Invalid file handle."; });
		return false;
	}

	LARGE_INTEGER distance{};
	distance.QuadPart = static_cast<LONGLONG>(size);

	if (unlikely(SetFilePointerEx(static_cast<HANDLE>(handle.data), distance, nullptr, FILE_BEGIN) == FALSE))
	{
		log.OutWarning([size](auto& ls)
		{ ls << "Failed to move the file pointer to " << size << ". Reason(" << GetLastError() << ')'; });

		return false;
	}

	if (unlikely(SetEndOfFile(static_cast<HANDLE>(handle.data)) == FALSE))
	{
		log.OutWarning([size](auto& ls)
		{ ls << "Failed to truncate the file with the size " << size << ". Reason(" << GetLastError() << ')'; });

		return false;
	}

	return true;
}

void* MapMemory(FileHandle& fileHandle, size_t size, ProtectionMode protection, size_t offset) noexcept
{
	using namespace hbe;
	using namespace StringUtil;
	static auto log = Logger::Get(ToFunctionName(__FUNCSIG__));

	if (unlikely(!fileHandle.IsValid()))
	{
		log.OutWarning([](auto& ls) { ls << "Invalid file handle."; });
		return nullptr;
	}

	if (unlikely(size == 0))
	{
		log.OutWarning([size](auto& ls) { ls << "Invalid size = " << size; });
		return nullptr;
	}

	const int protectionValue = protection.value;
	HANDLE mapping = CreateFileMappingA(static_cast<HANDLE>(fileHandle.data), nullptr,
		ToPageProtection(protectionValue), static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), nullptr);

	if (unlikely(mapping == nullptr))
	{
		log.OutError([size, protectionValue, offset](auto& ls)
		{
			ls << "Failed to create a file mapping. ErrorMsg(" << GetLastError()
			   << ") with arguments size = " << size << ", protection = " << protectionValue << ", offset = " << offset;
		});

		return nullptr;
	}

	void* ptr = MapViewOfFile(mapping, ToFileMapAccess(protectionValue), static_cast<DWORD>(offset >> 32),
		static_cast<DWORD>(offset), size);

	CloseHandle(mapping);

	if (unlikely(ptr == nullptr))
	{
		log.OutError([size, protectionValue, offset](auto& ls)
		{
			ls << "Failed to map the file to a memory address. ErrorMsg(" << GetLastError()
			   << ") with arguments size = " << size << ", protection = " << protectionValue << ", offset = " << offset;
		});

		return nullptr;
	}

	return ptr;
}

bool MapSync(void* ptr, size_t size, MapSyncMode syncMode) noexcept
{
	if (unlikely(ptr == nullptr))
		return false;

	if (unlikely(size == 0))
		return true;

	if (unlikely(FlushViewOfFile(ptr, size) == FALSE))
	{
		using namespace hbe;
		using namespace StringUtil;
		static auto log = Logger::Get(ToFunctionName(__FUNCSIG__));
		log.OutError([ptr](auto& ls)
		{ ls << "Failed to set sync. ptr = " << ptr << ", ErrorMsg(" << GetLastError() << ')'; });
	}

	return true;
}

bool UnmapMemory(void* ptr, size_t size) noexcept
{
	using namespace hbe;
	using namespace StringUtil;
	static auto log = Logger::Get(ToFunctionName(__FUNCSIG__));

	if (unlikely(ptr == nullptr))
	{
		log.OutWarning([](auto& ls) { ls << "Null pointer input."; });
		return false;
	}

	if (unlikely(UnmapViewOfFile(ptr) == FALSE))
	{
		log.OutError([ptr, size](auto& ls)
		{ ls << "Failed to unmap(" << ptr << ") with the size " << size << ". ErrorMsg(" << GetLastError() << ')'; });
	}

	return true;
}
} // namespace OS

#endif // PLATFORM_WINDOWS
