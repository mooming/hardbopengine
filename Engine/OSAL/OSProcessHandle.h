// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


namespace OS
{
class ProcessHandle final
{
public:
	union
	{
		void* data{};
		int pid;
	};

	ProcessHandle() noexcept;

	ProcessHandle(const ProcessHandle&) = delete;
	ProcessHandle(ProcessHandle&& rhs) noexcept;

	[[nodiscard]] bool IsValid() const noexcept;
	void Invalidate() noexcept;
};
} // namespace OS
