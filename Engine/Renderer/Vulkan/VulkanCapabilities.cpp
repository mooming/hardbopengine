// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "VulkanCapabilities.h"

#include <algorithm>
#include <cstring>

#include "Log/Logger.h"

namespace hbe::Renderer
{

namespace
{

constexpr uint32_t MaxInstanceExtensionProbe = 64;

DeviceType toDeviceType(VkPhysicalDeviceType type) noexcept
{
	switch (type)
	{
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			return DeviceType::DiscreteGpu;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			return DeviceType::IntegratedGpu;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			return DeviceType::VirtualGpu;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			return DeviceType::CpuSoftware;
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		default:
			return DeviceType::Unknown;
	}
}

/// @brief Copy a Vulkan fixed-size, null-terminated string into our shorter fixed buffer.
/// @details Truncated with the terminator always written, because a device name we later
///          print must never run past MaxDeviceNameLength.
void copyDeviceName(const char* source, char (&destination)[RenderCapabilities::MaxDeviceNameLength]) noexcept
{
	constexpr size_t MaxCopyLength = RenderCapabilities::MaxDeviceNameLength - 1;
	const size_t copyLength = std::min<std::size_t>(std::strlen(source), MaxCopyLength);

	std::memcpy(destination, source, copyLength);
	destination[copyLength] = '\0';
}

/// @brief Whether the Vulkan loader advertises an instance-level extension.
bool hasInstanceExtension(const char* extensionName) noexcept
{
	uint32_t availableCount = 0;
	if (vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr) != VK_SUCCESS || availableCount == 0)
	{
		return false;
	}

	VkExtensionProperties extensions[MaxInstanceExtensionProbe];
	// Vulkan writes back the number it actually filled, so this count must not be const.
	uint32_t probeCount = std::min<uint32_t>(availableCount, MaxInstanceExtensionProbe);
	if (vkEnumerateInstanceExtensionProperties(nullptr, &probeCount, extensions) != VK_SUCCESS)
	{
		return false;
	}

	for (uint32_t index = 0; index < probeCount; ++index)
	{
		if (std::strcmp(extensions[index].extensionName, extensionName) == 0)
		{
			return true;
		}
	}

	return false;
}

} // namespace

void fillRenderCapabilities(VkPhysicalDevice physicalDevice, RenderCapabilities& outCapabilities) noexcept
{
	static const auto log = Logger::get("VulkanCapabilities", ELogLevel::Error);

	// Start from "unknown" so a failed query can never leave stale or invented values behind.
	outCapabilities = RenderCapabilities();

	if (physicalDevice == VK_NULL_HANDLE)
	{
		log.outError("Error: cannot query capabilities from a null physical device; leaving capabilities unqueried");
		return;
	}

	// These two are the plain (non-chained) structs, so they carry no sType - zeroing them and
	// letting the driver write the whole record is the whole contract here.
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);

	VkPhysicalDeviceFeatures features{};
	vkGetPhysicalDeviceFeatures(physicalDevice, &features);

	// Identity.
	copyDeviceName(properties.deviceName, outCapabilities.deviceName);
	outCapabilities.deviceType = toDeviceType(properties.deviceType);
	outCapabilities.vendorId = properties.vendorID;
	outCapabilities.deviceId = properties.deviceID;
	outCapabilities.driverVersion = properties.driverVersion;
	outCapabilities.apiVersionMajor = static_cast<uint8_t>(VK_VERSION_MAJOR(properties.apiVersion));
	outCapabilities.apiVersionMinor = static_cast<uint8_t>(VK_VERSION_MINOR(properties.apiVersion));

	// Feature support - read straight from the device, so a driver that drops a feature shows it.
	outCapabilities.supportsTessellation = features.tessellationShader != VK_FALSE;
	outCapabilities.supportsGeometryShader = features.geometryShader != VK_FALSE;
	outCapabilities.supportsMultiDrawIndirect = features.multiDrawIndirect != VK_FALSE;
	outCapabilities.supports32BitIndices = features.fullDrawIndexUint32 != VK_FALSE;
	outCapabilities.supportsCubeTextureArrays = features.imageCubeArray != VK_FALSE;
	outCapabilities.supportsIndependentBlend = features.independentBlend != VK_FALSE;
	outCapabilities.supportsDualSourceBlend = features.dualSrcBlend != VK_FALSE;
	outCapabilities.supportsLogicOperations = features.logicOp != VK_FALSE;
	outCapabilities.supportsDepthClamp = features.depthClamp != VK_FALSE;
	outCapabilities.supportsDepthBiasClamp = features.depthBiasClamp != VK_FALSE;
	outCapabilities.supportsDepthBounds = features.depthBounds != VK_FALSE;
	outCapabilities.supportsWireframeFill = features.fillModeNonSolid != VK_FALSE;
	outCapabilities.supportsWideLines = features.wideLines != VK_FALSE;
	outCapabilities.supportsSamplerAnisotropy = features.samplerAnisotropy != VK_FALSE;
	outCapabilities.supportsPreciseOcclusionQueries = features.occlusionQueryPrecise != VK_FALSE;
	outCapabilities.supportsTextureGather = features.shaderImageGatherExtended != VK_FALSE;
	outCapabilities.supportsTextureCompressionBC = features.textureCompressionBC != VK_FALSE;
	outCapabilities.supportsTextureCompressionASTC = features.textureCompressionASTC_LDR != VK_FALSE;
	outCapabilities.supportsRobustBufferAccess = features.robustBufferAccess != VK_FALSE;

	// Compute is not a queryable feature bit in Vulkan: it is mandatory in core since 1.0, so
	// there is no VkPhysicalDeviceFeatures member for it. It stays true by definition.
	outCapabilities.supportsComputeShader = true;

	// Limits.
	const auto& limits = properties.limits;
	outCapabilities.maxTextureDimension1D = limits.maxImageDimension1D;
	outCapabilities.maxTextureDimension2D = limits.maxImageDimension2D;
	outCapabilities.maxTextureDimension3D = limits.maxImageDimension3D;
	outCapabilities.maxTextureDimensionCube = limits.maxImageDimensionCube;
	outCapabilities.maxTextureArrayLayers = limits.maxImageArrayLayers;
	outCapabilities.maxColorAttachments = limits.maxColorAttachments;
	outCapabilities.maxVertexAttributes = limits.maxVertexInputAttributes;
	outCapabilities.maxVertexBufferBindings = limits.maxVertexInputBindings;
	outCapabilities.maxVertexOutputComponents = limits.maxVertexOutputComponents;
	outCapabilities.maxUniformBufferBindings = limits.maxDescriptorSetUniformBuffers;
	outCapabilities.maxTextureBindings = limits.maxDescriptorSetSampledImages;
	outCapabilities.maxPushConstantBytes = limits.maxPushConstantsSize;
	outCapabilities.maxDrawIndirectCount = limits.maxDrawIndirectCount;
	outCapabilities.maxComputeWorkGroupInvocations = limits.maxComputeWorkGroupInvocations;
	outCapabilities.uniformBufferOffsetAlignment = static_cast<uint32_t>(limits.minUniformBufferOffsetAlignment);
	outCapabilities.maxSamplerAnisotropy = static_cast<float>(limits.maxSamplerAnisotropy);
	outCapabilities.maxSamplerLodBias = static_cast<float>(limits.maxSamplerLodBias);
	outCapabilities.timestampPeriodNanoseconds = static_cast<float>(limits.timestampPeriod);

	outCapabilities.isDeviceQueried = true;
}

bool queryDefaultDeviceCapabilities(RenderCapabilities& outCapabilities) noexcept
{
	static const auto log = Logger::get("VulkanCapabilities", ELogLevel::Warning);

	outCapabilities = RenderCapabilities();

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "HardBop Engine";
	appInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0);

	// MoltenVK publishes itself as a portability driver, which the loader hides unless
	// VK_KHR_portability_enumeration is both supported and enabled. Conforming drivers are not
	// required to offer it, and enabling an extension the loader does not have fails instance
	// creation outright, so it is enabled conditionally - unlike VulkanRenderer::CreateInstance,
	// which can assume a portability driver on macOS and asks for it unconditionally.
	const bool enablePortabilityEnumeration = hasInstanceExtension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
	const char* enabledExtensions[] = {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};

	VkInstanceCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	if (enablePortabilityEnumeration)
	{
		instanceInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		instanceInfo.enabledExtensionCount = 1;
		instanceInfo.ppEnabledExtensionNames = enabledExtensions;
	}

	VkInstance instance = VK_NULL_HANDLE;
	const VkResult createResult = vkCreateInstance(&instanceInfo, nullptr, &instance);
	if (createResult != VK_SUCCESS)
	{
		// No loader, no ICD, or an incompatible driver. That is a legitimate answer rather than
		// a fault, so it is a warning and the caller keeps an explicitly unqueried descriptor.
		log.outWarning([&createResult](auto& ls)
		{
			ls << "Warning: no Vulkan instance for the capability probe (VkResult=" << static_cast<int>(createResult)
			   << "); capabilities stay unqueried";
		});
		return false;
	}

	uint32_t deviceCount = 0;
	const VkResult countResult = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (countResult != VK_SUCCESS || deviceCount == 0)
	{
		log.outWarning([&countResult, &deviceCount](auto& ls)
		{
			ls << "Warning: Vulkan exposed no physical device (VkResult=" << static_cast<int>(countResult)
			   << ", count=" << deviceCount << "); capabilities stay unqueried";
		});
		vkDestroyInstance(instance, nullptr);
		return false;
	}

	// The first enumerated device is good enough for a pre-device question about the machine.
	// Actual device selection stays with VulkanRenderer::PickDevice, which additionally requires
	// a graphics queue that can present to the real surface - something this probe has none of.
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	deviceCount = 1;
	const VkResult fetchResult = vkEnumeratePhysicalDevices(instance, &deviceCount, &physicalDevice);
	if (fetchResult != VK_SUCCESS)
	{
		log.outWarning([&fetchResult](auto& ls)
		{
			ls << "Warning: vkEnumeratePhysicalDevices (fetch) failed (VkResult=" << static_cast<int>(fetchResult)
			   << "); capabilities stay unqueried";
		});
	}
	else
	{
		fillRenderCapabilities(physicalDevice, outCapabilities);
	}

	vkDestroyInstance(instance, nullptr);
	return outCapabilities.isDeviceQueried;
}

} // namespace hbe::Renderer
