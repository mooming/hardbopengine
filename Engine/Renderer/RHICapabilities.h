// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "RenderCapabilities.h"

#ifdef __UNIT_TEST__

#include "Test/TestCollection.h"

namespace hbe
{

class RHICapabilitiesTest : public TestCollection
{
public:
	RHICapabilitiesTest() : TestCollection("RHICapabilitiesTest") {}

protected:
	void Prepare() override;
};

} // namespace hbe

#endif

namespace hbe
{
namespace Renderer
{

/// @brief Answers RHI questions before a renderer exists, without committing callers to a backend.
/// @details The renderer is the authority once it owns a device - VulkanRenderer queries the
///          device it actually picked and caches the result, which is both cheaper and more
///          accurate than anything this stateless probe can do. Use this only for the
///          before-Initialize questions: is a backend available at all, and roughly what can
///          this machine do.
class RHICapabilities final
{
public:
	[[nodiscard]] static bool IsVulkanSupported() noexcept;

	/// @brief Probe the first available device, creating and discarding a Vulkan instance.
	/// @return A descriptor whose isDeviceQueried is false when nothing could be probed.
	[[nodiscard]] static RenderCapabilities GetCapabilities() noexcept;
};

} // namespace Renderer
} // namespace hbe
