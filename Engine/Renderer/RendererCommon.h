// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"


namespace hbe::Renderer
{
enum class APIType : uint8_t
{
	Unknown = 0,
	Vulkan,
	Metal,
	DX12,
	Software
};

using ResourceHandle = uint32_t;

enum class RHIResourceUsage : uint32_t
{
	Undefined = 0,
	ShaderResource,     // Read-only (SRV)
	UnorderedAccess,    // Read-Write (UAV)
	RenderTarget,       // Color attachment
	DepthStencil,       // Depth/Stencil attachment
	IndirectArgument,   // For indirect draw/dispatch
	ConstantBuffer      // UBO/CBV
};

enum PipelineStageFlags : uint32_t
{
	StageNone = 0,
	StageCompute = 1 << 0,
	StageVertex  = 1 << 1,
	StageFragment = 1 << 2,
	StageRayGen   = 1 << 3,
	StageClosestHit = 1 << 4,
	StageMiss     = 1 << 5,
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

class RenderCapabilities
{
public:
	APIType apiType;
	bool supportsGeometryShader;
	bool supportsTessellation;
	bool supportsComputeShader;
	bool supportsBindless;            // New: Bindless descriptor support
	uint32_t maxTextureSize;
	uint32_t maxUniformBuffers;
	uint32_t maxVertexAttribs;
	uint32_t maxBindlessResources;    // New: Limit for bindless arrays

	RenderCapabilities()
		: apiType(APIType::Unknown)
		, supportsGeometryShader(false)
		, supportsTessellation(false)
		, supportsComputeShader(false)
		, supportsBindless(false)
		, maxTextureSize(4096)
		, maxUniformBuffers(16)
		, maxVertexAttribs(16)
		, maxBindlessResources(0)
	{}
};

} // namespace hbe::Renderer
