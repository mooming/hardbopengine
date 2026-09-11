// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

namespace OS
{

class ProtectionMode final
{
public:
	int value;

	ProtectionMode() noexcept : value(0) {}

	void setForbidden() noexcept;
	void setReadable() noexcept;
	void setWritable() noexcept;
	void setExecutable() noexcept;
};

} // namespace OS
