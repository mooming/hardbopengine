// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RenderCapabilities.h"

namespace hbe::Renderer
{

/// @brief Builds an "unknown" descriptor: nothing queried, nothing supported, no limits.
/// @details Every field is zeroed on purpose. An unqueried descriptor must never look like a
///          real one - the previous revision shipped 4096/16/16 and supportsTessellation=false
///          as defaults, which real hardware (Apple M4 Pro: 16384/31/155 and tessellation
///          *supported*) contradicted, and callers had no way to tell the two apart.
///          When a field is added here, add it to this list too.
RenderCapabilities::RenderCapabilities()
	: deviceName{}
	, deviceType(DeviceType::Unknown)
	, vendorId(0)
	, deviceId(0)
	, driverVersion(0)
	, apiVersionMajor(0)
	, apiVersionMinor(0)
	, isDeviceQueried(false)
	, supportsComputeShader(false)
	, supportsTessellation(false)
	, supportsGeometryShader(false)
	, supportsMultiDrawIndirect(false)
	, supports32BitIndices(false)
	, supportsCubeTextureArrays(false)
	, supportsIndependentBlend(false)
	, supportsDualSourceBlend(false)
	, supportsLogicOperations(false)
	, supportsDepthClamp(false)
	, supportsDepthBiasClamp(false)
	, supportsDepthBounds(false)
	, supportsWireframeFill(false)
	, supportsWideLines(false)
	, supportsSamplerAnisotropy(false)
	, supportsPreciseOcclusionQueries(false)
	, supportsTextureGather(false)
	, supportsTextureCompressionBC(false)
	, supportsTextureCompressionASTC(false)
	, supportsRobustBufferAccess(false)
	, maxTextureDimension1D(0)
	, maxTextureDimension2D(0)
	, maxTextureDimension3D(0)
	, maxTextureDimensionCube(0)
	, maxTextureArrayLayers(0)
	, maxColorAttachments(0)
	, maxVertexAttributes(0)
	, maxVertexBufferBindings(0)
	, maxVertexOutputComponents(0)
	, maxUniformBufferBindings(0)
	, maxTextureBindings(0)
	, maxPushConstantBytes(0)
	, maxDrawIndirectCount(0)
	, maxComputeWorkGroupInvocations(0)
	, uniformBufferOffsetAlignment(0)
	, maxSamplerAnisotropy(0.0f)
	, maxSamplerLodBias(0.0f)
	, timestampPeriodNanoseconds(0.0f)
{
}

} // namespace hbe::Renderer
