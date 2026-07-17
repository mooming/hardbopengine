# Vulkan Renderer Implementation Guide

**Detailed step-by-step instructions for implementing the Minimal Vulkan Renderer**

---

## Prerequisites

Before starting, ensure you have:
- [ ] Vulkan SDK installed (Linux/macOS/Windows)
- [ ] CMake 3.12+ configured
- [ ] Basic C++23 knowledge
- [ ] Understanding of Vulkan concepts (optional but helpful)

**Verify Vulkan SDK is available:**
```bash
# Linux
vulkaninfo --summary

# macOS
vulkaninfo --summary

# Windows
"C:\VulkanSDK\bin\vulkaninfo64.exe" --summary
```

---

## Step 0: Project Setup

### 0.1 Create Directory Structure

```bash
cd Applications/TriangleExample
mkdir -p Shaders
```

### 0.2 Create Shader Files

**File: `Shaders/vertex.vert`**
```glsl
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragColor = inColor;
}
```

**File: `Shaders/fragment.frag`**
```glsl
#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
```

### 0.3 Update CMakeLists.txt

Add new source files:
```cmake
add_executable (TriangleExample
    Main.cpp
    MinimalVulkanRenderer.h
    MinimalVulkanRenderer.cpp
    VulkanComponents.h
    VulkanSystems.h
    VulkanPipeline.h
    VulkanPipeline.cpp
    ${PLATFORM_SOURCES}
)
```

### 0.4 Verify Compilation (Step 1)

Create empty stubs first to verify the build works before implementing anything.

---

## Step 1: Vulkan Instance Creation

### 1.1 What is VkInstance?

`VkInstance` is the root object in Vulkan. It represents the Vulkan library and holds application-level state.

### 1.2 Implementation

**Define required extensions:**
```cpp
const std::vector<const char*> instanceExtensions = {
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __linux__
    VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
    VK_KHR_MACOS_SURFACE_EXTENSION_NAME,
#elif defined(_WIN32)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
};
```

**Enable validation layers (optional but recommended):**
```cpp
#ifdef ENABLE_VALIDATION_LAYERS
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
#else
const std::vector<const char*> validationLayers = {};
#endif
```

**Create application info:**
```cpp
VkApplicationInfo appInfo{};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "Triangle Example";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.pEngineName = "HardBop Engine";
appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
appInfo.apiVersion = VK_API_VERSION_1_0;
```

**Create instance:**
```cpp
VkInstanceCreateInfo createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
createInfo.pApplicationInfo = &appInfo;
createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
createInfo.ppEnabledExtensionNames = instanceExtensions.data();
createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
createInfo.ppEnabledLayerNames = validationLayers.data();

VkInstance instance;
VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
if (result != VK_SUCCESS) {
    // Handle error
    return false;
}
```

**Enable debug utils (optional, for validation messages):**
```cpp
#ifdef ENABLE_VALIDATION_LAYERS
auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

if (vkCreateDebugUtilsMessengerEXT) {
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugCreateInfo.pfnUserCallback = DebugCallback;
    
    vkCreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger);
}
#endif
```

### 1.3 Verification

- `vkCreateInstance` returns `VK_SUCCESS`
- Validation messages appear in console (if enabled)
- No crashes

---

## Step 2: Surface Creation

### 2.1 What is VkSurfaceKHR?

`VkSurfaceKHR` represents the window surface where Vulkan will present images. Creation is platform-specific.

### 2.2 Implementation

**Get native window handle from OSAL:**
```cpp
intptr_t nativeHandle = window->GetNativeHandle();
```

**Create surface based on platform:**

```cpp
#ifdef __linux__
    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.connection = xcbConnection;
    surfaceCreateInfo.window = static_cast<xcb_window_t>(nativeHandle);
    vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
    
#elif defined(__APPLE__)
    VkMacOSSurfaceCreateInfoMVK surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    surfaceCreateInfo.pView = nsView;
    vkCreateMacOSSurfaceMVK(instance, &surfaceCreateInfo, nullptr, &surface);
    
#elif defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hwnd = reinterpret_cast<HWND>(nativeHandle);
    vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
#endif
```

### 2.3 Verification

- Surface handle is valid
- No validation errors about surface creation

---

## Step 3: Physical Device Selection

### 3.1 What is VkPhysicalDevice?

`VkPhysicalDevice` represents a GPU. You enumerate available devices and pick the best one.

### 3.2 Implementation

**Enumerate devices:**
```cpp
uint32_t deviceCount = 0;
vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

if (deviceCount == 0) {
    // Error: No GPUs with Vulkan support
    return false;
}

std::vector<VkPhysicalDevice> devices(deviceCount);
vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
```

**Pick the best device:**
```cpp
VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;

for (const auto& device : devices) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    
    // Prefer discrete GPUs
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        selectedDevice = device;
        break;
    }
    
    // Fallback to integrated GPUs
    if (selectedDevice == VK_NULL_HANDLE && 
        properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        selectedDevice = device;
    }
}

if (selectedDevice == VK_NULL_HANDLE) {
    // Error: No suitable GPU found
    return false;
}
```

**Check device features:**
```cpp
VkPhysicalDeviceFeatures features;
vkGetPhysicalDeviceFeatures(selectedDevice, &features);

if (!features.geometryShader) {
    // Error: Geometry shader not supported
    return false;
}
```

### 3.3 Verification

- At least one device found
- Selected device supports required features
- No validation errors

---

## Step 4: Logical Device Creation

### 4.1 What is VkDevice?

`VkDevice` is the primary object for device-level operations. It's created from a physical device.

### 4.2 Implementation

**Find queue families:**
```cpp
uint32_t queueFamilyCount = 0;
vkGetPhysicalDeviceQueueFamilyProperties(selectedDevice, &queueFamilyCount, nullptr);

std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
vkGetPhysicalDeviceQueueFamilyProperties(selectedDevice, &queueFamilyCount, queueFamilies.data());

uint32_t graphicsQueueFamily = UINT32_MAX;
uint32_t presentQueueFamily = UINT32_MAX;

for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        graphicsQueueFamily = i;
    }
    
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(selectedDevice, i, surface, &presentSupport);
    if (presentSupport) {
        presentQueueFamily = i;
    }
}

if (graphicsQueueFamily == UINT32_MAX || presentQueueFamily == UINT32_MAX) {
    // Error: Missing required queue families
    return false;
}
```

**Define queue create info:**
```cpp
std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
std::set<uint32_t> uniqueQueueFamilies = {graphicsQueueFamily, presentQueueFamily};

float queuePriority = 1.0f;
for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
}
```

**Create logical device:**
```cpp
VkDeviceCreateInfo deviceCreateInfo{};
deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
deviceCreateInfo.pEnabledFeatures = &features;

VkDevice device;
VkResult result = vkCreateDevice(selectedDevice, &deviceCreateInfo, nullptr, &device);
if (result != VK_SUCCESS) {
    // Error: Failed to create logical device
    return false;
}
```

**Get queue handles:**
```cpp
vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
```

### 4.3 Verification

- `vkCreateDevice` returns `VK_SUCCESS`
- Queue handles are valid
- No validation errors

---

## Step 5: Swapchain Creation

### 5.1 What is VkSwapchainKHR?

The swapchain manages the images presented to the screen. It's a chain of images the GPU renders to and the OS presents.

### 5.2 Implementation

**Query surface capabilities:**
```cpp
VkSurfaceCapabilitiesKHR capabilities;
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(selectedDevice, surface, &capabilities);
```

**Query surface formats:**
```cpp
uint32_t formatCount;
vkGetPhysicalDeviceSurfaceFormatsKHR(selectedDevice, surface, &formatCount, nullptr);

std::vector<VkSurfaceFormatKHR> formats(formatCount);
vkGetPhysicalDeviceSurfaceFormatsKHR(selectedDevice, surface, &formatCount, formats.data());

VkSurfaceFormatKHR surfaceFormat;
if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
    surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
} else {
    surfaceFormat = formats[0];
}
```

**Choose swapchain image count:**
```cpp
uint32_t imageCount = capabilities.minImageCount + 1;
if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
}

if (imageCount < capabilities.minImageCount) {
    imageCount = capabilities.minImageCount;
}
```

**Create swapchain:**
```cpp
VkSwapchainCreateInfoKHR swapchainCreateInfo{};
swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
swapchainCreateInfo.surface = surface;
swapchainCreateInfo.minImageCount = imageCount;
swapchainCreateInfo.imageFormat = surfaceFormat.format;
swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
swapchainCreateInfo.imageExtent = capabilities.currentExtent;
swapchainCreateInfo.imageArrayLayers = 1;
swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
swapchainCreateInfo.preTransform = capabilities.currentTransform;
swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
swapchainCreateInfo.clipped = VK_TRUE;
swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

VkSwapchainKHR swapchain;
VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain);
if (result != VK_SUCCESS) {
    // Error: Failed to create swapchain
    return false;
}
```

**Get swapchain images:**
```cpp
uint32_t swapchainImageCount;
vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);

std::vector<VkImage> swapchainImages(swapchainImageCount);
vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
```

### 5.3 Verification

- Swapchain created successfully
- Swapchain images retrieved
- No validation errors

---

## Step 6: Render Pass and Pipeline

### 6.1 What is VkRenderPass?

`VkRenderPass` defines the sequence of attachments and operations for rendering.

### 6.2 Implementation

**Define attachment:**
```cpp
VkAttachmentDescription attachment{};
attachment.format = surfaceFormat.format;
attachment.samples = VK_SAMPLE_COUNT_1_BIT;
attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
```

**Define subpass:**
```cpp
VkAttachmentReference colorAttachmentRef{};
colorAttachmentRef.attachment = 0;
colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

VkSubpassDescription subpass{};
subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
subpass.colorAttachmentCount = 1;
subpass.pColorAttachments = &colorAttachmentRef;
```

**Create render pass:**
```cpp
VkRenderPassCreateInfo renderPassCreateInfo{};
renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
renderPassCreateInfo.attachmentCount = 1;
renderPassCreateInfo.pAttachments = &attachment;
renderPassCreateInfo.subpassCount = 1;
renderPassCreateInfo.pSubpasses = &subpass;

VkRenderPass renderPass;
VkResult result = vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
if (result != VK_SUCCESS) {
    // Error: Failed to create render pass
    return false;
}
```

**Create graphics pipeline:**
```cpp
// Load shaders
std::vector<char> vertShaderCode = ReadFile("Shaders/vertex.spv");
std::vector<char> fragShaderCode = ReadFile("Shaders/fragment.spv");

VkShaderModule vertModule = CreateShaderModule(vertShaderCode);
VkShaderModule fragModule = CreateShaderModule(fragShaderCode);

// Pipeline layout
VkPipelineLayout pipelineLayout;
VkPipelineLayoutCreateInfo layoutCreateInfo{};
layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &pipelineLayout);

// Pipeline create info
VkPipelineShaderStageCreateInfo vertStage{};
vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
vertStage.module = vertModule;
vertStage.pName = "main";

VkPipelineShaderStageCreateInfo fragStage{};
fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
fragStage.module = fragModule;
fragStage.pName = "main";

VkPipelineVertexInputStateCreateInfo vertexInput{};
vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
// Define vertex input binding and attributes

VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

VkPipelineViewportStateCreateInfo viewportState{};
viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
viewportState.viewportCount = 1;
viewportState.scissorCount = 1;

VkPipelineRasterizationStateCreateInfo rasterization{};
rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterization.polygonMode = VK_POLYGON_MODE_FILL;
rasterization.cullMode = VK_CULL_MODE