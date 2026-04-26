// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "RendererCommon.h"
#include "OSAL/Window.h"

namespace hbe
{
namespace Renderer
{

class IRenderer
{
public:
	virtual ~IRenderer() = default;

	virtual bool Initialize(OS::IWindow* window) = 0;
	virtual void Shutdown() = 0;

	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual void Render(float deltaTime) = 0;

	virtual APIType GetAPIType() const = 0;
	virtual RenderCapabilities GetCapabilities() const = 0;

protected:
	IRenderer() = default;
};

} // namespace Renderer
} // namespace hbe