// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSProtectionMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#include <sys/mman.h>

namespace OS
{

void ProtectionMode::SetForbidden() noexcept { value |= PROT_NONE; }

void ProtectionMode::SetReadable() noexcept { value |= PROT_READ; }

void ProtectionMode::SetWritable() noexcept { value |= PROT_WRITE; }

void ProtectionMode::SetExecutable() noexcept { value |= PROT_EXEC; }

} // namespace OS

#endif // PLATFORM_OSX
