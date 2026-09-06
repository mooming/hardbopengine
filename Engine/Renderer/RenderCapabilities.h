// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Config/BuildConfig.h"

namespace hbe::Renderer
{

/// @brief What kind of physical device the driver is driving, in API-neutral terms.
enum class DeviceType : uint8_t
{
	Unknown = 0,
	DiscreteGpu,
	IntegratedGpu,
	VirtualGpu,
	CpuSoftware
};

/// @brief API-neutral snapshot of one graphics device: identity, feature support, limits.
/// @details Field names are deliberately chosen to be expressible by Vulkan, Direct3D 12 and
///          Metal alike, so a backend adapter only has to translate names, never reshape the
///          struct. The mapping each backend is expected to use:
///
/// - deviceName
///     Vulkan: deviceName  |  D3D12: DXGI_ADAPTER_DESC::Description  |  Metal: MTLDevice::name
/// - deviceType
///     Vulkan: deviceType  |  D3D12: DXGI_GPU_DESCRIPTOR  |  Metal: MTLGPUFamily / isLowPower
/// - vendorId / deviceId
///     Vulkan: vendorID / deviceID  |  D3D12: DXGI_ADAPTER_DESC  |  Metal: PCI vendorID / deviceID
/// - apiVersionMajor / Minor
///     Vulkan: VK_VERSION_MAJOR / MINOR  |  D3D12: D3D_FEATURE_LEVEL  |  Metal: MTLGPUFamily
/// - driverVersion
///     Vulkan: driverVersion  |  D3D12: DXGI driver version  |  Metal: not exposed, stays 0
/// - supportsComputeShader
///     Vulkan: core-mandatory in Vulkan 1.0  |  D3D12: DirectCompute  |  Metal: compute pipelines
/// - supportsTessellation
///     Vulkan: tessellationShader  |  D3D12: hull + domain shaders  |  Metal: patch control / evaluation
/// - supportsGeometryShader
///     Vulkan: geometryShader  |  D3D12: always supported  |  Metal: no geometry shader stage
/// - supportsMultiDrawIndirect
///     Vulkan: multiDrawIndirect  |  D3D12: indirect draws  |  Metal: indirect draw with buffer
/// - supports32BitIndices
///     Vulkan: fullDrawIndexUint32  |  D3D12: max vertex index 2^32-1  |  Metal: MTLIndexTypeUInt32
/// - supportsCubeTextureArrays
///     Vulkan: imageCubeArray  |  D3D12: cubemap resource arrays  |  Metal: MTLTextureTypeCubeArray
/// - supportsIndependentBlend
///     Vulkan: independentBlend  |  D3D12: write masks per target  |  Metal: per-attachment blend state
/// - supportsDualSourceBlend
///     Vulkan: dualSrcBlend  |  D3D12: blend with blend factor  |  Metal: MTLBlendFactor stage reads
/// - supportsLogicOperations
///     Vulkan: logicOp  |  D3D12: supported from FL 11_1  |  Metal: not supported
/// - supportsDepthClamp
///     Vulkan: depthClamp  |  D3D12: depth clip  |  Metal: MTLDepthClipMode::clamp
/// - supportsDepthBiasClamp
///     Vulkan: depthBiasClamp  |  D3D12: depth bias clamp  |  Metal: depthBiasClamp
/// - supportsDepthBounds
///     Vulkan: depthBounds  |  D3D12: depth bounds test  |  Metal: not supported
/// - supportsWireframeFill
///     Vulkan: fillModeNonSolid  |  D3D12: fill mode wireframe  |  Metal: MTLPolygonFillMode
/// - supportsWideLines
///     Vulkan: wideLines  |  D3D12: line width greater than 1  |  Metal: lineWidth, 1 on Apple GPUs
/// - supportsSamplerAnisotropy
///     Vulkan: samplerAnisotropy  |  D3D12: max anisotropy 16  |  Metal: anisotropic filtering
/// - supportsPreciseOcclusionQueries
///     Vulkan: occlusionQueryPrecise  |  D3D12: occlusion queries  |  Metal: visibility result buffer
/// - supportsTextureGather
///     Vulkan: shaderImageGatherExtended  |  D3D12: Gather  |  Metal: texture::gather
/// - supportsTextureCompressionBC
///     Vulkan: textureCompressionBC  |  D3D12: BC1 to BC7  |  Metal: BC from Apple7 GPUs
/// - supportsTextureCompressionASTC
///     Vulkan: textureCompressionASTC_LDR  |  D3D12: optional from FL 12_2  |  Metal: ASTC, not on macOS GPUs
/// - supportsRobustBufferAccess
///     Vulkan: robustBufferAccess  |  D3D12: always robust  |  Metal: always bounds-locked
/// - maxTextureDimension1D
///     Vulkan: maxImageDimension1D  |  D3D12: max texture dimension 1D  |  Metal: MTLDevice::maxTextureSize
/// - maxTextureDimension2D
///     Vulkan: maxImageDimension2D  |  D3D12: max texture dimension 16384  |  Metal: MTLDevice::maxTextureSize
/// - maxTextureDimension3D
///     Vulkan: maxImageDimension3D  |  D3D12: max volume extent 2048  |  Metal: MTLTextureType3D
/// - maxTextureDimensionCube
///     Vulkan: maxImageDimensionCube  |  D3D12: max cubemap dimension  |  Metal: MTLDevice::maxTextureSize
/// - maxTextureArrayLayers
///     Vulkan: maxImageArrayLayers  |  D3D12: texture array axis dim  |  Metal: layer count
/// - maxColorAttachments
///     Vulkan: maxColorAttachments  |  D3D12: simultaneous render targets, 8  |  Metal: colorAttachments[8]
/// - maxVertexAttributes
///     Vulkan: maxVertexInputAttributes  |  D3D12: max input element descriptions  |  Metal: 31 per vertex descriptor
/// - maxVertexBufferBindings
///     Vulkan: maxVertexInputBindings  |  D3D12: max input slots, 32  |  Metal: buffers per stage
/// - maxVertexOutputComponents
///     Vulkan: maxVertexOutputComponents  |  D3D12: output component count  |  Metal: 32 x float4
/// - maxUniformBufferBindings
///     Vulkan: maxDescriptorSetUniformBuffers  |  D3D12: constant buffer bindings  |  Metal: buffers per stage
/// - maxTextureBindings
///     Vulkan: maxDescriptorSetSampledImages  |  D3D12: shader resource bindings  |  Metal: textures per stage
/// - maxPushConstantBytes
///     Vulkan: maxPushConstantsSize  |  D3D12: root constants  |  Metal: inline data / buffer length
/// - maxDrawIndirectCount
///     Vulkan: maxDrawIndirectCount  |  D3D12: indirect argument count  |  Metal: indirect buffer size
/// - maxComputeWorkGroupInvocations
///     Vulkan: maxComputeWorkGroupInvocations  |  D3D12: wave lane count  |  Metal: maxThreadsPerThreadgroup
/// - uniformBufferOffsetAlignment
///     Vulkan: minUniformBufferOffsetAlignment  |  D3D12: 256B placement alignment  |  Metal: per-buffer alignment
/// - maxSamplerAnisotropy
///     Vulkan: maxSamplerAnisotropy  |  D3D12: max anisotropy  |  Metal: maxAnisotropy
/// - maxSamplerLodBias
///     Vulkan: maxSamplerLodBias  |  D3D12: mip LOD bias range  |  Metal: sampler LOD bias
/// - timestampPeriodNanoseconds
///     Vulkan: timestampPeriod  |  D3D12: timestamp frequency  |  Metal: GPU timestamp ticks
///
///          Every zero / false means "not queried" rather than "unsupported" - see
///          isDeviceQueried. Adapters must set isDeviceQueried only on success.
class RenderCapabilities
{
public:
	static constexpr size_t MaxDeviceNameLength = 128;

	// Identity: which device answered.
	char deviceName[MaxDeviceNameLength];
	DeviceType deviceType;
	uint32_t vendorId;
	uint32_t deviceId;
	uint32_t driverVersion;
	uint8_t apiVersionMajor;
	uint8_t apiVersionMinor;

	// Feature support - one bit each, kept as a single contiguous run so they share
	// storage units instead of burning a byte apiece.
	// Bit-field caveats, since these are addressable-looking but are not:
	//   - no address-of: &caps.supportsComputeShader will not compile.
	//   - bit order within a storage unit is implementation-defined, so this struct is a
	//     runtime-only description. Never serialise it, memcpy it between processes, or
	//     upload it to a shader - go through the fields explicitly instead.
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

	// Limits.
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

/// @brief The descriptor is passed by value and copied out of the backend adapter, so it
///        must stay a plain, trivially copyable type with no ownership inside it.
static_assert(std::is_trivially_copyable_v<RenderCapabilities>, "RenderCapabilities must stay trivially copyable");

} // namespace hbe::Renderer
