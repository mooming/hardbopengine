// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "HSTL/HString.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include <memory>

namespace hbe
{
namespace Renderer
{

enum class APIType : uint8_t
{
	Unknown = 0,
	Vulkan,
	Metal,
	DX12
};

struct Vertex
{
	Vector3<float> position;
	Vector4<float> color;

	Vertex() = default;

	Vertex(float x, float y, float z, float r, float g, float b, float a)
		: position(x, y, z)
		, color(r, g, b, a)
	{}
};

struct RenderCapabilities
{
	APIType apiType;
	bool supportsGeometryShader;
	bool supportsTessellation;
	bool supportsComputeShader;
	uint32_t maxTextureSize;
	uint32_t maxUniformBuffers;
	uint32_t maxVertexAttribs;

	RenderCapabilities()
		: apiType(APIType::Unknown)
		, supportsGeometryShader(false)
		, supportsTessellation(false)
		, supportsComputeShader(false)
		, maxTextureSize(4096)
		, maxUniformBuffers(16)
		, maxVertexAttribs(16)
	{}
};

} // namespace Renderer
} // namespace hbe