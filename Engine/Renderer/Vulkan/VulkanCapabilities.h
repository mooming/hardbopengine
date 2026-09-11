// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <vulkan/vulkan.h>

#include "Config/BuildConfig.h"
#include "RenderCapabilities.h"

namespace hbe::Renderer
{

/// @brief Translate one Vulkan physical device into the API-neutral capability descriptor.
/// @details The single place that knows Vulkan's spelling of a capability. Backends are chosen
///          by macros at compile time, the way OSAL does it: one neutral RenderCapabilities
///          contract, an adapter like this one per backend directory, and a macro decides which
///          adapter gets compiled. A Direct3D 12 or Metal backend therefore adds its own
///          translator writing the same descriptor, rather than extending this one or adding a
///          runtime API-kind enum.
/// @param physicalDevice Device to query. VK_NULL_HANDLE leaves outCapabilities untouched.
/// @param outCapabilities Filled in; on success isDeviceQueried is set true.
void fillRenderCapabilities(VkPhysicalDevice physicalDevice, RenderCapabilities& outCapabilities) noexcept;

/// @brief Query the first usable Vulkan device without owning an instance, surface or device.
/// @details For the before-Initialize question "what is this machine capable of" - it creates a
///          throwaway instance, reads the first physical device and tears the instance down.
///          A renderer that already holds a device should use FillRenderCapabilities instead,
///          which costs one call and no instance churn.
/// @param outCapabilities Filled in; isDeviceQueried stays false when nothing could be probed.
/// @return True when a device answered, false when Vulkan is unavailable here.
bool queryDefaultDeviceCapabilities(RenderCapabilities& outCapabilities) noexcept;

} // namespace hbe::Renderer
