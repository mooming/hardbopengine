// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <vulkan/vulkan.h>

#include "Config/BuildConfig.h"
#include "RenderCapabilities.h"

namespace hbe::Renderer
{
/// API reference: docs/Renderer/VulkanCapabilities/index.html
void FillRenderCapabilities(VkPhysicalDevice physicalDevice, RenderCapabilities& outCapabilities) noexcept;

bool QueryDefaultDeviceCapabilities(RenderCapabilities& outCapabilities) noexcept;
} // namespace hbe::Renderer
