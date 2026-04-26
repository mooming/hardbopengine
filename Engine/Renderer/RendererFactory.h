// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "RendererCommon.h"
#include "RHICapabilities.h"

namespace hbe
{
namespace Renderer
{

class IRenderer;
class RendererFactory
{
public:
	static std::unique_ptr<IRenderer> Create(APIType preferredAPI = APIType::Unknown);
	static std::unique_ptr<IRenderer> CreateWithFallback();
};

} // namespace Renderer
} // namespace hbe