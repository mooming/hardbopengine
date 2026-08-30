// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "RendererCommon.h"

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

/// @brief Static queries about the single supported RHI (Vulkan).
class RHICapabilities final
{
public:
	[[nodiscard]] static bool IsVulkanSupported() noexcept;

	[[nodiscard]] static APIType GetPreferredAPI() noexcept;
	[[nodiscard]] static RenderCapabilities GetCapabilities() noexcept;
};

} // namespace Renderer
} // namespace hbe
