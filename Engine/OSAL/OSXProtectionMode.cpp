// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSProtectionMode.h"

#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#include <sys/mman.h>

namespace OS
{

void ProtectionMode::setForbidden() noexcept { value |= PROT_NONE; }

void ProtectionMode::setReadable() noexcept { value |= PROT_READ; }

void ProtectionMode::setWritable() noexcept { value |= PROT_WRITE; }

void ProtectionMode::setExecutable() noexcept { value |= PROT_EXEC; }

} // namespace OS

#endif // PLATFORM_OSX
