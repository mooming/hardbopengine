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

class RHICapabilities final
{
public:
	[[nodiscard]] static bool IsVulkanSupported() noexcept;
	[[nodiscard]] static bool IsMetalSupported() noexcept;
	[[nodiscard]] static bool IsDX12Supported() noexcept;

	[[nodiscard]] static APIType GetPreferredAPI() noexcept;
	[[nodiscard]] static RenderCapabilities GetCapabilities(APIType api) noexcept;

private:
	static bool CheckVulkanSupport() noexcept;
	static bool CheckMetalSupport() noexcept;
	static bool CheckDX12Support() noexcept;

	static bool vulkanChecked;
	static bool metalChecked;
	static bool dx12Checked;
	static bool vulkanSupported;
	static bool metalSupported;
	static bool dx12Supported;
};

} // namespace Renderer
} // namespace hbe