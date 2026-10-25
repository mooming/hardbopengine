// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "RenderCapabilities.h"


namespace hbe
{
namespace Renderer
{
/// API reference: docs/Renderer/RHICapabilities/index.html
class RHICapabilities final
{
public:
	[[nodiscard]] static bool IsVulkanSupported() noexcept;

	[[nodiscard]] static RenderCapabilities GetCapabilities() noexcept;
};
} // namespace Renderer
} // namespace hbe

#ifdef TEST_ENABLED

#include "Test/TestCollection.h"

namespace hbe
{
class RHICapabilitiesTest : public TestCollection
{
public:
	RHICapabilitiesTest()
		: TestCollection("RHICapabilitiesTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe

#endif
