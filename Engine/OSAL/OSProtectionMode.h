// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

namespace OS
{

	struct ProtectionMode final
	{
		int value = 0;

		void SetForbidden();
		void SetReadable();
		void SetWritable();
		void SetExecutable();
	};

} // namespace OS
