// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Test/TestCollection.h"
#include "Renderer/RendererCommon.h"
#include "Renderer/RHICapabilities.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h"

namespace hbe
{

class RendererTest : public TestCollection
{
public:
	RendererTest() : TestCollection("RendererTest") {}

protected:
	void Prepare() override;
};

void RendererTest::Prepare()
{
 AddCase("RHICapabilities", []() {
		constexpr auto api = Renderer::APIType::Metal;
		auto caps = Renderer::RHICapabilities::GetCapabilities(api);

		Assert(caps.apiType == api, "API type mismatch");
	});

	AddCase("GetPreferredAPI", []() {
		auto api = Renderer::RHICapabilities::GetPreferredAPI();
		Assert(api != Renderer::APIType::Unknown, "No renderer API available");
	});

	AddCase("RendererFactory", []() {
		auto renderer = Renderer::RendererFactory::CreateWithFallback();
		Assert(renderer != nullptr, "Renderer creation failed");
	});
}

} // namespace hbe