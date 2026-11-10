// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>

#include "OSProcessHandle.h"
#include "OSProcessWaitResult.h"

namespace OS
{
class FileHandle;

struct ProcessSpawnDesc
{
};

class Process final
{
private:
	union
	{
		void* data;
		int pid;
	};

public:
	Process();
	void Kill() const;

	[[nodiscard]] bool IsValid() const;
	[[nodiscard]] bool Spawn(const ProcessSpawnDesc& desc);
	[[nodiscard]] const FileHandle& GetStdIn() const noexcept;
	[[nodiscard]] const FileHandle& GetStdOut() const noexcept;
	[[nodiscard]] const FileHandle& GetStdErr() const noexcept;
	[[nodiscard]] EProcessWaitResult Wait(uint32_t timeoutMilliSecs, int& exitCode) noexcept;
};
} // namespace OS

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class OSProcessTest final : public TestCollection
{
public:
	OSProcessTest()
		: TestCollection("OSProcessTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif // TEST_ENABLED
