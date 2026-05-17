// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RendererTest.h"


namespace hbe
{

void RendererTest::Prepare()
{
	AddCase("RHICapabilities", []()
	{
		constexpr auto api = Renderer::APIType::Metal;
		auto caps = Renderer::RHICapabilities::GetCapabilities(api);

		Assert(caps.apiType == api, "API type mismatch");
	});

	AddCase("GetPreferredAPI", []()
	{
		auto api = Renderer::RHICapabilities::GetPreferredAPI();

		Assert(api != Renderer::APIType::Unknown, "No renderer API available");
	});

	AddCase("RendererFactory", []()
	{
		auto renderer = Renderer::RendererFactory::CreateWithFallback();

		Assert(renderer != nullptr, "Renderer creation failed");
	});
}

} // namespace hbe
