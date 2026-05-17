// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "RendererCommon.h"
#include "RHICapabilities.h"

namespace hbe
{
namespace Renderer
{

class IRenderer;
class RendererFactory final
{
public:
	[[nodiscard]] static std::unique_ptr<IRenderer> Create(APIType preferredAPI = APIType::Unknown) noexcept;
	[[nodiscard]] static std::unique_ptr<IRenderer> CreateWithFallback() noexcept;
};

} // namespace Renderer
} // namespace hbe