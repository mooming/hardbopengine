// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


namespace OS
{
class FileOpenMode final
{
public:
	int value;

	FileOpenMode() noexcept
		: value(0)
	{
	}

	void SetReadOnly() noexcept;
	void SetWriteOnly() noexcept;
	void SetReadWrite() noexcept;
	void SetCreate() noexcept;
	void SetTruncate() noexcept;
	void SetAppend() noexcept;
};
} // namespace OS
