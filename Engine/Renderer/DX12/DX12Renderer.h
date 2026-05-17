// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "IRenderer.h"

namespace hbe
{
namespace Renderer
{

struct DX12Vertex {
    float x, y;
    float r, g, b, a;
};

class DX12Renderer final : public IRenderer
{
public:
	DX12Renderer() noexcept;
	~DX12Renderer() override;

	[[nodiscard]] bool Initialize(OS::IWindow* window) noexcept override;
	void Shutdown() noexcept override;

	void BeginFrame() noexcept override;
	void EndFrame() noexcept override;

	void Render(float deltaTime) noexcept override;

	[[nodiscard]] APIType GetAPIType() const noexcept override;
	[[nodiscard]] RenderCapabilities GetCapabilities() const noexcept override;

private:
	OS::IWindow* window;
	APIType apiType;
	RenderCapabilities capabilities;

	intptr_t device;
	intptr_t commandQueue;
	intptr_t commandList;
	intptr_t vertexBuffer;

	float rotationAngle;
	bool initialized;
};

} // namespace Renderer
} // namespace hbe