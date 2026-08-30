// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __UNIT_TEST__
#include "RendererTest.h"

#include "RendererCommon.h"
#include "RHICapabilities.h"
#include "Vulkan/VulkanRenderer.h"

namespace hbe
{
using namespace Renderer;

RendererTest::RendererTest() noexcept
	:TestCollection("RendererTest")
{
}

void RendererTest::Prepare()
{
	AddTest("VulkanRenderer API Type", [](auto& log)
	{
		VulkanRenderer renderer;

		log << "API Type: " << static_cast<int>(renderer.GetAPIType());
		Assert(renderer.GetAPIType() == APIType::Vulkan, "VulkanRenderer should report Vulkan");
	});

	AddTest("VulkanRenderer Capabilities", [](auto& log)
	{
		VulkanRenderer renderer;
		RenderCapabilities caps = renderer.GetCapabilities();

		log << "maxTextureSize: " << caps.maxTextureSize
		    << ", supportsComputeShader: " << caps.supportsComputeShader;
		Assert(caps.apiType == APIType::Vulkan, "Capabilities API type should be Vulkan");
		Assert(caps.supportsComputeShader, "Vulkan should support compute shaders");
	});

	AddTest("VulkanRenderer Round Trip", [](auto& log)
	{
		VulkanRenderer renderer;
		OS::Window* window = nullptr;

		Assert(renderer.Initialize(window), "Initialize should succeed with a null window");
		renderer.Render(0.016f);
		Assert(renderer.Initialize(window), "Second Initialize should return true (no-op)");
		renderer.Shutdown();
	});

	AddTest("RHICapabilities Preferred API", [](auto& log)
	{
		auto api = RHICapabilities::GetPreferredAPI();

		log << "Preferred: " << static_cast<int>(api);
		Assert(api == APIType::Vulkan, "Preferred API should be Vulkan");
	});
}

} // namespace hbe
#endif // __UNIT_TEST__
