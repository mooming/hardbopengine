// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "IRenderer.h"

namespace hbe
{
namespace Renderer
{

class VulkanRenderer final : public IRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer() override;

	bool Initialize(OS::IWindow* window) override;
	void Shutdown() override;

	void BeginFrame() override;
	void EndFrame() override;

	void Render(float deltaTime) override;

	APIType GetAPIType() const override;
	RenderCapabilities GetCapabilities() const override;

private:
	OS::IWindow* m_Window = nullptr;
	APIType m_APIType = APIType::Vulkan;
	RenderCapabilities m_Capabilities;

	intptr_t m_Instance = 0;
	float m_RotationAngle = 0.0f;
	bool m_Initialized = false;
};

} // namespace Renderer
} // namespace hbe