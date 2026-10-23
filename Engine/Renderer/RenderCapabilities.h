// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Config/BuildConfig.h"

namespace hbe::Renderer
{
/// API reference: docs/Renderer/DeviceType/index.html
enum class DeviceType : uint8_t
{
	Unknown = 0,
	DiscreteGpu,
	IntegratedGpu,
	VirtualGpu,
	CpuSoftware
};

/// API reference: docs/Renderer/RenderCapabilities/index.html
class RenderCapabilities
{
public:
	static constexpr size_t MaxDeviceNameLength = 128;

	char deviceName[MaxDeviceNameLength];
	DeviceType deviceType;
	uint32_t vendorId;
	uint32_t deviceId;
	uint32_t driverVersion;
	uint8_t apiVersionMajor;
	uint8_t apiVersionMinor;

	bool isDeviceQueried : 1;

	bool supportsComputeShader : 1;
	bool supportsTessellation : 1;
	bool supportsGeometryShader : 1;
	bool supportsMultiDrawIndirect : 1;
	bool supports32BitIndices : 1;
	bool supportsCubeTextureArrays : 1;
	bool supportsIndependentBlend : 1;
	bool supportsDualSourceBlend : 1;
	bool supportsLogicOperations : 1;
	bool supportsDepthClamp : 1;
	bool supportsDepthBiasClamp : 1;
	bool supportsDepthBounds : 1;
	bool supportsWireframeFill : 1;
	bool supportsWideLines : 1;
	bool supportsSamplerAnisotropy : 1;
	bool supportsPreciseOcclusionQueries : 1;
	bool supportsTextureGather : 1;
	bool supportsTextureCompressionBC : 1;
	bool supportsTextureCompressionASTC : 1;
	bool supportsRobustBufferAccess : 1;

	uint32_t maxTextureDimension1D;
	uint32_t maxTextureDimension2D;
	uint32_t maxTextureDimension3D;
	uint32_t maxTextureDimensionCube;
	uint32_t maxTextureArrayLayers;
	uint32_t maxColorAttachments;
	uint32_t maxVertexAttributes;
	uint32_t maxVertexBufferBindings;
	uint32_t maxVertexOutputComponents;
	uint32_t maxUniformBufferBindings;
	uint32_t maxTextureBindings;
	uint32_t maxPushConstantBytes;
	uint32_t maxDrawIndirectCount;
	uint32_t maxComputeWorkGroupInvocations;
	uint32_t uniformBufferOffsetAlignment;
	float maxSamplerAnisotropy;
	float maxSamplerLodBias;
	float timestampPeriodNanoseconds;

	RenderCapabilities();
};

static_assert(std::is_trivially_copyable_v<RenderCapabilities>, "RenderCapabilities must stay trivially copyable");
} // namespace hbe::Renderer
