// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "OSAL/Window.h"
#include "RendererCommon.h"

namespace hbe
{
namespace Renderer
{

class IRenderer
{
public:
	virtual ~IRenderer() = default;

	[[nodiscard]] virtual bool Initialize(OS::IWindow* window) noexcept = 0;
	virtual void Shutdown() noexcept = 0;

	virtual void BeginFrame() noexcept = 0;
	virtual void EndFrame() noexcept = 0;

	virtual void Render(float deltaTime) noexcept = 0;

	[[nodiscard]] virtual APIType GetAPIType() const noexcept = 0;
	[[nodiscard]] virtual RenderCapabilities GetCapabilities() const noexcept = 0;

protected:
	IRenderer() noexcept = default;
};

} // namespace Renderer
} // namespace hbe