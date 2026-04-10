// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSProtectionMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_LINUX
#include <sys/mman.h>

namespace OS
{

	void ProtectionMode::SetForbidden() { value |= PROT_NONE; }

	void ProtectionMode::SetReadable() { value |= PROT_READ; }

	void ProtectionMode::SetWritable() { value |= PROT_WRITE; }

	void ProtectionMode::SetExecutable() { value |= PROT_EXEC; }

} // namespace OS

#endif // PLATFORM_LINUX
