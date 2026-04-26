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

class RHICapabilities
{
public:
	static bool IsVulkanSupported();
	static bool IsMetalSupported();
	static bool IsDX12Supported();

	static APIType GetPreferredAPI();
	static RenderCapabilities GetCapabilities(APIType api);

private:
	static bool CheckVulkanSupport();
	static bool CheckMetalSupport();
	static bool CheckDX12Support();

	static bool s_VulkanChecked;
	static bool s_MetalChecked;
	static bool s_DX12Checked;
	static bool s_VulkanSupported;
	static bool s_MetalSupported;
	static bool s_DX12Supported;
};

} // namespace Renderer
} // namespace hbe