// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Renderer/IRenderer.h"
#include "Renderer/RendererCommon.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/RHICapabilities.h"
#include "Test/TestCollection.h"


namespace hbe
{

class RendererTest final : public TestCollection
{
public:
	RendererTest() noexcept;

protected:
	void Prepare() override;
};

} // namespace hbe