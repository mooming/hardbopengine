// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Vulkan/VulkanRenderer.h"

#include "Core/CommonMacros.h"
#include "Log/Logger.h"
#include "ShadersSpv.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(PLATFORM_WINDOWS) && defined(VK_USE_PLATFORM_WIN32_KHR)
#include <vulkan/vulkan_win32.h>
#endif

namespace hbe
{
namespace Renderer
{

namespace
{
// Portability enumeration is required for MoltenVK to appear in the device list.
constexpr const char* kInstanceExtensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#if defined(PLATFORM_OSX)
	VK_EXT_METAL_SURFACE_EXTENSION_NAME,
#elif defined(PLATFORM_WINDOWS) && defined(VK_USE_PLATFORM_WIN32_KHR)
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
};

/// @brief Report a Vulkan call failure - engine logging rule: errors are always on.
void LogFailure(const char* step, VkResult result) noexcept
{
	static const auto log = Logger::Get("VulkanRenderer", ELogLevel::Error);
	log.OutError([step, result](auto& ls)
	{
		ls << step << " failed (VkResult=" << static_cast<int>(result) << ")";
	});
}

/// @brief Report a VulkanRenderer step that failed without a VkResult to report.
void LogFailure(const char* step) noexcept
{
	static const auto log = Logger::Get("VulkanRenderer", ELogLevel::Error);
	log.OutError(step);
}

constexpr uint32_t kInstanceExtensionCount = static_cast<uint32_t>(sizeof(kInstanceExtensions) / sizeof(const char*));

bool FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties,
	uint32_t& outIndex) noexcept
{
	VkPhysicalDeviceMemoryProperties mem{};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mem);
	for (uint32_t i = 0; i < mem.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1u << i)) != 0 && (mem.memoryTypes[i].propertyFlags & properties) == properties)
		{
			outIndex = i;
			return true;
		}
	}
	return false;
}

/// @brief out = lhs * rhs for column-major 4x4 matrices (element col*4+row).
/// @details The shader takes a pre-combined viewProj so that the 128-byte push range can
///          also carry the model matrix; this is the one multiply that buys us that room.
void MultiplyColumnMajor(float out[16], const float lhs[16], const float rhs[16]) noexcept
{
	float tmp[16];
	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k)
			{
				sum += lhs[k * 4 + row] * rhs[col * 4 + k];
			}
			tmp[col * 4 + row] = sum;
		}
	}
	std::memcpy(out, tmp, sizeof(tmp));
}

bool HasDepthSupport(VkPhysicalDevice physicalDevice, VkFormat format) noexcept
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
	return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0
		|| (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}
} // namespace

VulkanRenderer::VulkanRenderer() noexcept
	: window(nullptr)
	, initialized(false)
	, apiType(APIType::Vulkan)
	, capabilities()
	, instance(VK_NULL_HANDLE)
	, physicalDevice(VK_NULL_HANDLE)
	, queueFamilyIndex(0)
	, device(VK_NULL_HANDLE)
	, graphicsQueue(VK_NULL_HANDLE)
	, surface(VK_NULL_HANDLE)
	, surfaceFormat{}
	, presentMode(VK_PRESENT_MODE_FIFO_KHR)
	, extent{}
	, swapchain(VK_NULL_HANDLE)
	, depthFormat(VK_FORMAT_UNDEFINED)
	, depthImage(VK_NULL_HANDLE)
	, depthMemory(VK_NULL_HANDLE)
	, depthImageView(VK_NULL_HANDLE)
	, renderPass(VK_NULL_HANDLE)
	, commandPool(VK_NULL_HANDLE)
	, pipelineLayout(VK_NULL_HANDLE)
	, graphicsPipeline(VK_NULL_HANDLE)
	, vertModule(VK_NULL_HANDLE)
	, fragModule(VK_NULL_HANDLE)
	, vertexBuffer(VK_NULL_HANDLE)
	, vertexMemory(VK_NULL_HANDLE)
	, indexBuffer(VK_NULL_HANDLE)
	, indexMemory(VK_NULL_HANDLE)
	, vertexCount(0)
	, indexCount(0)
	, currentFrame(0)
	, imageIndex(0)
	, frameActive(false)
	, firstFramePresented(false)
{
	IdentityMatrix(modelMat);
	IdentityMatrix(viewMat);
	IdentityMatrix(projMat);
}

VulkanRenderer::~VulkanRenderer()
{
	Shutdown();
}

bool VulkanRenderer::Initialize(OS::Window* inWindow) noexcept
{
	if (initialized) return true;
	if (inWindow == nullptr) return false;

	window = inWindow;

	// The surface must exist before device picking: queue family support is
	// queried against the surface itself.
	if (!CreateInstance()) return false;
	if (!CreateSurface()) return false;
	if (!PickDevice()) return false;
	if (!CreateDevice()) return false;
	if (!CreateSwapchain()) return false;
	if (!CreateRenderPass()) return false;
	if (!CreateDepthResource()) return false;
	if (!CreateFramebuffers()) return false;
	if (!CreateCommandBuffers()) return false;
	if (!CreatePipeline()) return false;
	if (!CreateSyncObjects()) return false;

	initialized = true;
	return true;
}

void VulkanRenderer::Shutdown() noexcept
{
	returnIf(!initialized);

	Destroy();
	initialized = false;
}

bool VulkanRenderer::CreateInstance() noexcept
{
	VkApplicationInfo app{};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "HardBop VulkanRenderer";
	app.pEngineName = "HardBop Engine";
	app.apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0);

	VkInstanceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &app;
	ci.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	ci.enabledExtensionCount = kInstanceExtensionCount;
	ci.ppEnabledExtensionNames = kInstanceExtensions;

	const VkResult result = vkCreateInstance(&ci, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		LogFailure("vkCreateInstance", result);
		return false;
	}

	return true;
}

bool VulkanRenderer::CreateSurface() noexcept
{
#if defined(PLATFORM_OSX)
	return CreateMetalSurface();
#elif defined(PLATFORM_WINDOWS) && defined(VK_USE_PLATFORM_WIN32_KHR)
	// NOTE: compile-unverified on this machine (macOS only environment).
	VkWin32SurfaceCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	info.hwnd = reinterpret_cast<HWND>(window->GetNativeHandle());
	return vkCreateWin32SurfaceKHR(instance, &info, nullptr, &surface) == VK_SUCCESS;
#else
	// No surface backend for this platform yet. Linux needs OS::Window to expose its
	// X11 Display* (GetNativeHandle only returns the Window id).
	return false;
#endif
}

#if !defined(PLATFORM_OSX)
// The macOS implementation lives in VulkanRenderer.mm. This stub must stay excluded on
// macOS: a second definition here would satisfy the symbol from VulkanRenderer.cpp.o, so
// VulkanRenderer.mm.o would never be pulled out of the static archive and the real Metal
// surface would silently never run.
bool VulkanRenderer::CreateMetalSurface() noexcept
{
	return false;
}
#endif // !PLATFORM_OSX

bool VulkanRenderer::PickDevice() noexcept
{
	uint32_t count = 0;
	const VkResult enumResult = vkEnumeratePhysicalDevices(instance, &count, nullptr);
	if (enumResult != VK_SUCCESS)
	{
		LogFailure("vkEnumeratePhysicalDevices", enumResult);
		return false;
	}

	if (count == 0)
	{
		// Usually means the portability enumeration flag is missing or no ICD is installed.
		LogFailure("no Vulkan physical device found (check VK_ICD_FILENAMES / MoltenVK install)");
		return false;
	}

	std::vector<VkPhysicalDevice> devices(count);
	const VkResult fetchResult = vkEnumeratePhysicalDevices(instance, &count, devices.data());
	if (fetchResult != VK_SUCCESS)
	{
		LogFailure("vkEnumeratePhysicalDevices (fetch)", fetchResult);
		return false;
	}

	for (uint32_t i = 0; i < count; ++i)
	{
		uint32_t qfCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &qfCount, nullptr);
		std::vector<VkQueueFamilyProperties> queues(qfCount);
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &qfCount, queues.data());

		for (uint32_t q = 0; q < qfCount; ++q)
		{
			if ((queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue;

			VkBool32 presents = VK_FALSE;
			continueIf(vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], q, surface, &presents) != VK_SUCCESS);
			continueIf(presents != VK_TRUE);

			physicalDevice = devices[i];
			queueFamilyIndex = q;
			return true;
		}
	}

	LogFailure("no queue family supports graphics + presentation for this surface");
	return false;
}

bool VulkanRenderer::CreateDevice() noexcept
{
	float priority = 1.0f;
	VkDeviceQueueCreateInfo queueCI{};
	queueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCI.queueFamilyIndex = queueFamilyIndex;
	queueCI.queueCount = 1;
	queueCI.pQueuePriorities = &priority;

	// No optional features are required by this pipeline, so none are requested:
	// asking for an unsupported feature makes vkCreateDevice fail outright.
	// VK_KHR_swapchain must be enabled per device: without it the loader leaves the
	// swapchain entry points NULL and vkCreateSwapchainKHR fails.
	const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkDeviceCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.queueCreateInfoCount = 1;
	ci.pQueueCreateInfos = &queueCI;
	ci.enabledExtensionCount = 1;
	ci.ppEnabledExtensionNames = deviceExtensions;

	const VkResult result = vkCreateDevice(physicalDevice, &ci, nullptr, &device);
	if (result != VK_SUCCESS)
	{
		LogFailure("vkCreateDevice", result);
		return false;
	}

	vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);
	return true;
}

bool VulkanRenderer::CreateSwapchain() noexcept
{
	VkSurfaceCapabilitiesKHR caps{};
	const VkResult capsResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);
	if (capsResult != VK_SUCCESS)
	{
		LogFailure("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", capsResult);
		return false;
	}

	// currentExtent is 0xFFFFFFFF when the surface sizes itself from the window.
	if (caps.currentExtent.width == 0xFFFFFFFFu)
	{
		extent.width = static_cast<uint32_t>(window->GetWidth());
		extent.height = static_cast<uint32_t>(window->GetHeight());
		extent.width = (extent.width < caps.minImageExtent.width ? caps.minImageExtent.width : extent.width);
		extent.width = (extent.width > caps.maxImageExtent.width ? caps.maxImageExtent.width : extent.width);
		extent.height = (extent.height < caps.minImageExtent.height ? caps.minImageExtent.height : extent.height);
		extent.height = (extent.height > caps.maxImageExtent.height ? caps.maxImageExtent.height : extent.height);
	}
	else
	{
		extent = caps.currentExtent;
	}

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	if (formatCount > 0) vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
	if (formats.empty())
	{
		LogFailure("surface reports no supported formats");
		return false;
	}

	// The CAMetalLayer is configured as BGRA8Unorm - match it when offered.
	surfaceFormat = formats[0];
	for (const auto& f : formats)
	{
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM)
		{
			surfaceFormat = f;
			break;
		}
	}

	uint32_t modeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, nullptr);
	std::vector<VkPresentModeKHR> modes(modeCount);
	if (modeCount > 0) vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, modes.data());

	// FIFO is the only mode guaranteed to exist; mailbox is preferred when available.
	presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& m : modes)
	{
		if (m == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = m;
			break;
		}
	}

	uint32_t imageCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

	VkSwapchainCreateInfoKHR ci{};
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.surface = surface;
	ci.minImageCount = imageCount;
	ci.imageFormat = surfaceFormat.format;
	ci.imageColorSpace = surfaceFormat.colorSpace;
	ci.imageExtent = extent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.preTransform = caps.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.presentMode = presentMode;
	ci.clipped = VK_TRUE;
	ci.oldSwapchain = VK_NULL_HANDLE;

	const VkResult createResult = vkCreateSwapchainKHR(device, &ci, nullptr, &swapchain);
	if (createResult != VK_SUCCESS)
	{
		LogFailure("vkCreateSwapchainKHR", createResult);
		return false;
	}

	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
	swapchainImages.resize(imageCount);

	swapchainImageViews.assign(imageCount, VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchainImages.size(); ++i)
	{
		VkImageViewCreateInfo vci{};
		vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vci.image = swapchainImages[i];
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = surfaceFormat.format;
		vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vci.subresourceRange.baseMipLevel = 0;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.baseArrayLayer = 0;
		vci.subresourceRange.layerCount = 1;
		const VkResult viewResult = vkCreateImageView(device, &vci, nullptr, &swapchainImageViews[i]);
		if (viewResult != VK_SUCCESS)
		{
			LogFailure("vkCreateImageView (swapchain)", viewResult);
			return false;
		}
	}

	return true;
}

bool VulkanRenderer::CreateRenderPass() noexcept
{
	// Both descriptions must be contiguous: pAttachments is indexed, not per-element bound.
	VkAttachmentDescription attachments[2] = {};

	attachments[0].format = surfaceFormat.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	depthFormat = VK_FORMAT_D32_SFLOAT;
	if (!HasDepthSupport(physicalDevice, depthFormat)) depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
	if (!HasDepthSupport(physicalDevice, depthFormat)) depthFormat = VK_FORMAT_D16_UNORM;

	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef{};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthRef{};
	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;

	// External -> subpass 0: the acquire semaphore is waited outside the pass, so the
	// pass itself must not start writing colour/depth before the previous present ended.
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	ci.attachmentCount = 2;
	ci.pAttachments = attachments;
	ci.subpassCount = 1;
	ci.pSubpasses = &subpass;
	ci.dependencyCount = 1;
	ci.pDependencies = &dependency;

	const VkResult result = vkCreateRenderPass(device, &ci, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		LogFailure("vkCreateRenderPass", result);
		return false;
	}

	return true;
}

bool VulkanRenderer::CreateDepthResource() noexcept
{
	VkImageCreateInfo ici{};
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.extent = {extent.width, extent.height, 1};
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.format = depthFormat;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const VkResult imageResult = vkCreateImage(device, &ici, nullptr, &depthImage);
	if (imageResult != VK_SUCCESS)
	{
		LogFailure("vkCreateImage (depth)", imageResult);
		return false;
	}

	VkMemoryRequirements req{};
	vkGetImageMemoryRequirements(device, depthImage, &req);

	VkMemoryAllocateInfo ami{};
	ami.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ami.allocationSize = req.size;
	if (!FindMemoryType(physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ami.memoryTypeIndex))
	{
		LogFailure("no device local memory type for the depth image");
		vkDestroyImage(device, depthImage, nullptr);
		depthImage = VK_NULL_HANDLE;
		return false;
	}
	const VkResult allocResult = vkAllocateMemory(device, &ami, nullptr, &depthMemory);
	if (allocResult != VK_SUCCESS)
	{
		LogFailure("vkAllocateMemory (depth)", allocResult);
		return false;
	}
	const VkResult bindResult = vkBindImageMemory(device, depthImage, depthMemory, 0);
	if (bindResult != VK_SUCCESS)
	{
		LogFailure("vkBindImageMemory (depth)", bindResult);
		return false;
	}

	VkImageViewCreateInfo vci{};
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = depthImage;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = depthFormat;
	vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	vci.subresourceRange.baseMipLevel = 0;
	vci.subresourceRange.levelCount = 1;
	vci.subresourceRange.baseArrayLayer = 0;
	vci.subresourceRange.layerCount = 1;
	const VkResult viewResult = vkCreateImageView(device, &vci, nullptr, &depthImageView);
	if (viewResult != VK_SUCCESS)
	{
		LogFailure("vkCreateImageView (depth)", viewResult);
		return false;
	}

	return true;
}

bool VulkanRenderer::CreateFramebuffers() noexcept
{
	framebuffers.assign(swapchainImageViews.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchainImageViews.size(); ++i)
	{
		const VkImageView attachments[2] = {swapchainImageViews[i], depthImageView};
		VkFramebufferCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		ci.renderPass = renderPass;
		ci.attachmentCount = 2;
		ci.pAttachments = attachments;
		ci.width = extent.width;
		ci.height = extent.height;
		ci.layers = 1;
		const VkResult result = vkCreateFramebuffer(device, &ci, nullptr, &framebuffers[i]);
		if (result != VK_SUCCESS)
		{
			LogFailure("vkCreateFramebuffer", result);
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::CreateCommandBuffers() noexcept
{
	VkCommandPoolCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = queueFamilyIndex;
	const VkResult poolResult = vkCreateCommandPool(device, &ci, nullptr, &commandPool);
	if (poolResult != VK_SUCCESS)
	{
		LogFailure("vkCreateCommandPool", poolResult);
		return false;
	}

	// One command buffer per frame in flight: a submitted buffer must not be re-recorded.
	commandBuffers.assign(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = commandPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	const VkResult result = vkAllocateCommandBuffers(device, &ai, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		LogFailure("vkAllocateCommandBuffers", result);
		return false;
	}

	return true;
}

bool VulkanRenderer::CreateBuffer(VkBuffer& buffer, VkDeviceMemory& memory, size_t size,
	VkBufferUsageFlags usage) noexcept
{
	if (size == 0) return false;

	VkBufferCreateInfo bci{};
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = static_cast<VkDeviceSize>(size);
	bci.usage = usage;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	const VkResult createResult = vkCreateBuffer(device, &bci, nullptr, &buffer);
	if (createResult != VK_SUCCESS)
	{
		LogFailure("vkCreateBuffer", createResult);
		return false;
	}

	VkMemoryRequirements req{};
	vkGetBufferMemoryRequirements(device, buffer, &req);

	VkMemoryAllocateInfo ami{};
	ami.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ami.allocationSize = req.size;
	// The mesh is written once from the CPU and never modified per frame, so host
	// visible memory is mapped directly. A device local copy + staging transfer is the
	// right long-term shape (tracked as a follow-up).
	const auto properties = static_cast<VkMemoryPropertyFlags>(
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (!FindMemoryType(physicalDevice, req.memoryTypeBits, properties, ami.memoryTypeIndex))
	{
		LogFailure("no host visible memory type for the mesh buffer");
		vkDestroyBuffer(device, buffer, nullptr);
		buffer = VK_NULL_HANDLE;
		return false;
	}
	const VkResult result = vkAllocateMemory(device, &ami, nullptr, &memory);
	if (result != VK_SUCCESS)
	{
		LogFailure("vkAllocateMemory (buffer)", result);
		return false;
	}

	return true;
}

bool VulkanRenderer::CreateVertexBuffers(const Mesh& mesh) noexcept
{
	ReleaseVertexBuffers();

	vertexCount = mesh.vertices.size();
	indexCount = mesh.indices.size();
	if (vertexCount == 0 || indexCount == 0) return true; // empty mesh: clear-only frame

	const auto vertexBytes = vertexCount * sizeof(MeshVertex);
	const auto indexBytes = indexCount * sizeof(uint32_t);

	if (!CreateBuffer(vertexBuffer, vertexMemory, vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) return false;
	if (!CreateBuffer(indexBuffer, indexMemory, indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) return false;

	if (vkBindBufferMemory(device, vertexBuffer, vertexMemory, 0) != VK_SUCCESS)
	{
		LogFailure("vkBindBufferMemory (vertex)");
		return false;
	}
	if (vkBindBufferMemory(device, indexBuffer, indexMemory, 0) != VK_SUCCESS)
	{
		LogFailure("vkBindBufferMemory (index)");
		return false;
	}

	auto upload = [&](VkDeviceMemory mem, const void* data, size_t size) noexcept -> bool
	{
		void* dst = nullptr;
		const VkResult mapResult = vkMapMemory(device, mem, 0, static_cast<VkDeviceSize>(size), 0, &dst);
		if (mapResult != VK_SUCCESS)
		{
			LogFailure("vkMapMemory", mapResult);
			return false;
		}
		std::memcpy(dst, data, size);
		vkUnmapMemory(device, mem);
		return true;
	};

	if (!upload(vertexMemory, mesh.vertices.data(), vertexBytes)) return false;
	if (!upload(indexMemory, mesh.indices.data(), indexBytes)) return false;

	return true;
}

void VulkanRenderer::ReleaseVertexBuffers() noexcept
{
	if (indexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, indexBuffer, nullptr);
	if (indexMemory != VK_NULL_HANDLE) vkFreeMemory(device, indexMemory, nullptr);
	if (vertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(device, vertexBuffer, nullptr);
	if (vertexMemory != VK_NULL_HANDLE) vkFreeMemory(device, vertexMemory, nullptr);

	indexBuffer = VK_NULL_HANDLE;
	indexMemory = VK_NULL_HANDLE;
	vertexBuffer = VK_NULL_HANDLE;
	vertexMemory = VK_NULL_HANDLE;
	vertexCount = 0;
	indexCount = 0;
}

bool VulkanRenderer::CreatePipeline() noexcept
{
	auto loadModule = [&](const unsigned char* spv, size_t size, VkShaderModule& out) noexcept -> bool
	{
		VkShaderModuleCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = size;
		ci.pCode = reinterpret_cast<const uint32_t*>(spv);
		const VkResult result = vkCreateShaderModule(device, &ci, nullptr, &out);
		if (result != VK_SUCCESS)
		{
			LogFailure("vkCreateShaderModule", result);
			return false;
		}

		return true;
	};

	if (!loadModule(gMCVertexSpv, gMCVertexSpv_size, vertModule)) return false;
	if (!loadModule(gMCFragmentSpv, gMCFragmentSpv_size, fragModule)) return false;

	VkVertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(MeshVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	const VkVertexInputAttributeDescription attrs[2] = {
		{0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(MeshVertex, position))},
		{1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(MeshVertex, normal))},
	};

	VkPipelineVertexInputStateCreateInfo vi{};
	vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vi.vertexBindingDescriptionCount = 1;
	vi.pVertexBindingDescriptions = &binding;
	vi.vertexAttributeDescriptionCount = 2;
	vi.pVertexAttributeDescriptions = attrs;

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertModule;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragModule;
	stages[1].pName = "main";

	VkPipelineInputAssemblyStateCreateInfo iast{};
	iast.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	iast.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo vpst{};
	vpst.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vpst.viewportCount = 1;
	vpst.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rast{};
	rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rast.polygonMode = VK_POLYGON_MODE_FILL;
	// Marching-cubes winding is not guaranteed, so nothing is culled.
	rast.cullMode = VK_CULL_MODE_NONE;
	rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rast.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo mss{};
	mss.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	mss.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo dss{};
	dss.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dss.depthTestEnable = VK_TRUE;
	dss.depthWriteEnable = VK_TRUE;
	dss.depthCompareOp = VK_COMPARE_OP_LESS;
	dss.depthBoundsTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState cba{};
	cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	cba.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo cb{};
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = &cba;

	const VkDynamicState dynStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dsc{};
	dsc.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dsc.dynamicStateCount = 2;
	dsc.pDynamicStates = dynStates;

	VkPushConstantRange pcr{};
	pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pcr.offset = 0;
	pcr.size = sizeof(PushConstants);

	VkPipelineLayoutCreateInfo plci{};
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	const VkResult layoutResult = vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout);
	if (layoutResult != VK_SUCCESS)
	{
		LogFailure("vkCreatePipelineLayout", layoutResult);
		return false;
	}

	VkGraphicsPipelineCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	ci.stageCount = 2;
	ci.pStages = stages;
	ci.pVertexInputState = &vi;
	ci.pInputAssemblyState = &iast;
	ci.pViewportState = &vpst;
	ci.pRasterizationState = &rast;
	ci.pMultisampleState = &mss;
	ci.pDepthStencilState = &dss;
	ci.pColorBlendState = &cb;
	ci.pDynamicState = &dsc;
	ci.layout = pipelineLayout;
	ci.renderPass = renderPass;
	ci.subpass = 0;

	const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		LogFailure("vkCreateGraphicsPipelines", result);
		return false;
	}

	return true;
}

bool VulkanRenderer::CreateSyncObjects() noexcept
{
	semaphoresImageAvailable.assign(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	semaphoresRenderFinished.assign(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	fences.assign(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Fences start signaled: frame 0 must not block on work that was never submitted.
	VkFenceCreateInfo fi{};
	fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		if (vkCreateSemaphore(device, &si, nullptr, &semaphoresImageAvailable[i]) != VK_SUCCESS)
		{
			LogFailure("vkCreateSemaphore (image available)");
			return false;
		}
		if (vkCreateSemaphore(device, &si, nullptr, &semaphoresRenderFinished[i]) != VK_SUCCESS)
		{
			LogFailure("vkCreateSemaphore (render finished)");
			return false;
		}
		if (vkCreateFence(device, &fi, nullptr, &fences[i]) != VK_SUCCESS)
		{
			LogFailure("vkCreateFence");
			return false;
		}
	}

	return true;
}

void VulkanRenderer::RecordFrame() noexcept
{
	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	const VkResult beginResult = vkBeginCommandBuffer(commandBuffers[currentFrame], &bi);
	if (beginResult != VK_SUCCESS)
	{
		LogFailure("vkBeginCommandBuffer", beginResult);
		return;
	}

	VkClearValue clears[2] = {};
	clears[0].color = {{0.06f, 0.08f, 0.12f, 1.0f}};
	clears[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo ri{};
	ri.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	ri.renderPass = renderPass;
	ri.framebuffer = framebuffers[imageIndex];
	ri.renderArea.offset = {0, 0};
	ri.renderArea.extent = extent;
	ri.pClearValues = clears;
	ri.clearValueCount = 2;
	vkCmdBeginRenderPass(commandBuffers[currentFrame], &ri, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	// Viewport and scissor are dynamic state - they must be set before drawing.
	VkViewport vp{};
	vp.x = 0.0f;
	vp.y = 0.0f;
	vp.width = static_cast<float>(extent.width);
	vp.height = static_cast<float>(extent.height);
	vp.minDepth = 0.0f;
	vp.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &vp);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

	if (indexCount > 0 && vertexCount > 0)
	{
		PushConstants push{};
		std::memcpy(push.model, modelMat, sizeof(push.model));
		MultiplyColumnMajor(push.viewProj, projMat, viewMat);
		vkCmdPushConstants(commandBuffers[currentFrame], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push),
			&push);

		const VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, &vertexBuffer, &offset);
		vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffers[currentFrame], static_cast<uint32_t>(indexCount), 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffers[currentFrame]);
	vkEndCommandBuffer(commandBuffers[currentFrame]);
}

void VulkanRenderer::BeginFrame() noexcept
{
	returnIf(!initialized);

	vkWaitForFences(device, 1, &fences[currentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &fences[currentFrame]);

	frameActive = false;

	const VkResult acquire = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
		semaphoresImageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
	{
		// Swapchain lost. EndFrame still has to release the fence it just reset,
		// otherwise the next BeginFrame would wait on a fence nobody signals.
		// Recreating the swapchain here is the real fix (tracked as a follow-up).
		return;
	}

	frameActive = imageIndex < static_cast<uint32_t>(framebuffers.size());
}

void VulkanRenderer::Render(float /*deltaTime*/) noexcept
{
	returnIf(!(initialized && frameActive));

	RecordFrame();
}

void VulkanRenderer::EndFrame() noexcept
{
	returnIf(!initialized);

	if (frameActive)
	{
		const VkSemaphore waitSem = semaphoresImageAvailable[currentFrame];
		const VkSemaphore signalSem = semaphoresRenderFinished[currentFrame];
		const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submit{};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores = &waitSem;
		submit.pWaitDstStageMask = &waitStage;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &commandBuffers[currentFrame];
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &signalSem;

		if (vkQueueSubmit(graphicsQueue, 1, &submit, fences[currentFrame]) != VK_SUCCESS)
		{
			LogFailure("vkQueueSubmit");
			// The fence was reset in BeginFrame: release it even though the submit failed.
			const VkSubmitInfo release{};
			vkQueueSubmit(graphicsQueue, 0, &release, fences[currentFrame]);
			return;
		}

		const VkSwapchainKHR sc = swapchain;
		VkPresentInfoKHR present{};
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.waitSemaphoreCount = 1;
		present.pWaitSemaphores = &signalSem;
		present.swapchainCount = 1;
		present.pSwapchains = &sc;
		present.pImageIndices = &imageIndex;
		const VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &present);
		if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR)
		{
			LogFailure("vkQueuePresentKHR", presentResult);
		}
		else if (!firstFramePresented)
		{
			// One milestone line: proof that the whole pipeline reached presentation.
			firstFramePresented = true;
			static const auto log = Logger::Get("VulkanRenderer");
			log.Out([&](auto& ls)
			{
				ls << "first frame presented (" << extent.width << "x" << extent.height
					<< ", swapchain images=" << framebuffers.size()
					<< ", index count=" << indexCount << ")";
			});
		}
	}
	else
	{
		// Nothing was drawn this frame: release the fence with an empty submit so the
		// frame cycle stays intact.
		VkSubmitInfo submit{};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		if (vkQueueSubmit(graphicsQueue, 0, &submit, fences[currentFrame]) != VK_SUCCESS)
		{
			LogFailure("vkQueueSubmit (fence release)");
			return;
		}
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::SetMesh(const Mesh& mesh) noexcept
{
	returnIf(!initialized);

	if (!CreateVertexBuffers(mesh))
	{
		LogFailure("mesh upload failed - drawing clear colour only");
		vertexCount = 0;
		indexCount = 0;
	}
}

void VulkanRenderer::SetModel(const float* m) noexcept
{
	std::memcpy(modelMat, m, sizeof(modelMat));
}

void VulkanRenderer::SetView(const float* m) noexcept
{
	std::memcpy(viewMat, m, sizeof(viewMat));
}

void VulkanRenderer::SetProj(const float* m) noexcept
{
	std::memcpy(projMat, m, sizeof(projMat));
}

VkExtent2D VulkanRenderer::GetExtent() const noexcept
{
	return extent;
}

APIType VulkanRenderer::GetAPIType() const noexcept
{
	return apiType;
}

RenderCapabilities VulkanRenderer::GetCapabilities() const noexcept
{
	return capabilities;
}

void VulkanRenderer::Destroy() noexcept
{
	if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);

	for (auto sem : semaphoresImageAvailable)
		if (sem != VK_NULL_HANDLE) vkDestroySemaphore(device, sem, nullptr);
	for (auto sem : semaphoresRenderFinished)
		if (sem != VK_NULL_HANDLE) vkDestroySemaphore(device, sem, nullptr);
	for (auto fence : fences)
		if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr);
	semaphoresImageAvailable.clear();
	semaphoresRenderFinished.clear();
	fences.clear();

	ReleaseVertexBuffers();

	// The command pool frees its command buffers when destroyed.
	if (commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, commandPool, nullptr);
	commandPool = VK_NULL_HANDLE;
	commandBuffers.clear();

	if (graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, graphicsPipeline, nullptr);
	if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, vertModule, nullptr);
	if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, fragModule, nullptr);
	graphicsPipeline = VK_NULL_HANDLE;
	pipelineLayout = VK_NULL_HANDLE;
	vertModule = VK_NULL_HANDLE;
	fragModule = VK_NULL_HANDLE;

	for (auto fb : framebuffers)
		if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device, fb, nullptr);
	framebuffers.clear();

	// Views must die before the image / memory they alias.
	if (depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, depthImageView, nullptr);
	if (depthImage != VK_NULL_HANDLE) vkDestroyImage(device, depthImage, nullptr);
	if (depthMemory != VK_NULL_HANDLE) vkFreeMemory(device, depthMemory, nullptr);
	depthImageView = VK_NULL_HANDLE;
	depthImage = VK_NULL_HANDLE;
	depthMemory = VK_NULL_HANDLE;

	for (auto view : swapchainImageViews)
		if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
	swapchainImageViews.clear();
	swapchainImages.clear();

	if (swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapchain, nullptr);
	swapchain = VK_NULL_HANDLE;

	if (device != VK_NULL_HANDLE) vkDestroyDevice(device, nullptr);
	device = VK_NULL_HANDLE;

	if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, nullptr);
	if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, nullptr);
	surface = VK_NULL_HANDLE;
	instance = VK_NULL_HANDLE;

	physicalDevice = VK_NULL_HANDLE;
	graphicsQueue = VK_NULL_HANDLE;
	frameActive = false;
	currentFrame = 0;
	imageIndex = 0;
}

} // namespace Renderer
} // namespace hbe
