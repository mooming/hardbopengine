// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef TEST_ENABLED
#include "RendererTest.h"

#include "RenderCapabilities.h"
#include "Vulkan/VulkanRenderer.h"

namespace hbe
{
using namespace Renderer;

RendererTest::RendererTest() noexcept
	: TestCollection("RendererTest")
{
}

void RendererTest::Prepare()
{
	AddTest("VulkanRenderer capabilities are unqueried before Initialize", [](auto& log)
	{
		VulkanRenderer renderer;
		const RenderCapabilities caps = renderer.GetCapabilities();

		log << "isDeviceQueried=" << caps.isDeviceQueried << " maxTextureDimension2D=" << caps.maxTextureDimension2D;

		Assert(!caps.isDeviceQueried, "A renderer without a device must not claim queried capabilities");
		Assert(caps.maxTextureDimension2D == 0, "A renderer without a device must not report a texture limit");
		Assert(caps.deviceName[0] == '\0', "A renderer without a device must not report a device name");
	});

	AddTest("VulkanRenderer capabilities match an unknown descriptor", [](auto& log)
	{
		VulkanRenderer renderer;
		const RenderCapabilities caps = renderer.GetCapabilities();
		const RenderCapabilities unknown;

		log << "sizeof(RenderCapabilities)=" << sizeof(RenderCapabilities);

		Assert(caps.isDeviceQueried == unknown.isDeviceQueried, "Unqueried flag must match");
		Assert(caps.deviceType == unknown.deviceType, "Unqueried device type must match");
		Assert(caps.maxTextureDimension2D == unknown.maxTextureDimension2D, "Unqueried 2D limit must match");
		Assert(caps.maxVertexAttributes == unknown.maxVertexAttributes, "Unqueried vertex limit must match");
		Assert(caps.supportsComputeShader == unknown.supportsComputeShader, "Unqueried compute flag must match");
	});

	AddTest("VulkanRenderer Round Trip", [](auto& log)
	{
		VulkanRenderer renderer;
		OS::Window* window = nullptr;

		Assert(!renderer.Initialize(window), "Initialize must reject a null window");
		renderer.Render(0.016f);
		Assert(!renderer.GetCapabilities().isDeviceQueried, "A rejected Initialize must leave capabilities unqueried");
		renderer.Shutdown();

		log << "null-window round trip is a safe no-op";
	});
}
} // namespace hbe
#endif // TEST_ENABLED
