================================================================================
                    VulkanTriangle Application Specification
================================================================================

## 1. Overview

A simple triangle rendering application using Vulkan API, integrated with the HardBop Engine.

## 2. Technical Requirements

### Platform Support
- Windows (primary)
- macOS (via MoltenVK)
- Linux (if Vulkan available)

### Dependencies
- GLFW (already in project)
- Vulkan SDK (required for compilation)
- HardBop Engine modules: Engine, Memory, OSAL, Log, Resource

### Build Configuration
- Minimum Vulkan API: 1.0
- Target: Vulkan 1.2+ if available

## 3. Core Features

### 3.1 Window Management
- Create window via GLFW
- Initialize Vulkan instance
- Create swapchain with double buffering
- Handle window resize events

### 3.2 Rendering Pipeline
- Vertex buffer with 3 vertices (RGB triangle)
- Simple fragment shader (solid color or gradient)
- Single render pass
- Command buffer recording and submission

### 3.3 Engine Integration
- Use Engine for initialization and main loop
- Register with TaskSystem for render updates
- Use Memory module for buffer allocations
- Use Log for debug output

### 3.4 Window Modes
The application supports three window modes:
- **WINDOW**: Regular windowed mode (default)
- **FULLSCREEN**: Exclusive fullscreen mode
- **FULLSCREEN_WINDOWED**: Borderless window covering entire screen

Mode selection via command line argument:
- No args or "window" → WINDOW mode
- "fullscreen" → FULLSCREEN mode
- "fullscreen windowed" or "borderless" → FULLSCREEN_WINDOWED mode

### 3.5 User Input
- **Exit on any key press**: Application terminates when user presses any key
- Use GLFW's `glfwGetKey()` to detect key press in render loop

## 4. Application Structure

```
Applications/VulkanTriangle/
├── CMakeLists.txt          - Build configuration
├── VulkanTriangle.cpp     - Main application entry point
├── Resources/
│   ├── Shaders/           - GLSL/VK shaders (compiled at runtime or embedded)
│   └── Meshes/            - Vertex data
└── README.md              - Application documentation
```

## 5. Implementation Details

### 5.1 Minimal Vulkan Setup
```
1. glfwInit() + glfwCreateWindow()
2. vkEnumerateInstanceExtensionProperties() - check Vulkan support
3. vkCreateInstance()
4. Create device and queue
5. Create swapchain (2 images)
6. Create render pass
7. Create pipeline (shader stages, viewport, rasterization)
8. Create framebuffers
9. Create command buffers
```

### 5.2 Render Loop
```
// Parse command line for window mode
WindowMode mode = WindowMode::Window;
if (argc > 1) {
    if (strcmp(argv[1], "fullscreen") == 0) mode = WindowMode::Fullscreen;
    else if (strcmp(argv[1], "borderless") == 0) mode = WindowMode::FullscreenWindowed;
}

while (!glfwWindowShouldClose()) {
    glfwPollEvents();
    
    // Check for key press - exit on any key
    if (glfwGetKey(window, GLFW_KEY_UNKNOWN) != GLFW_RELEASE) {
        break; // Exit loop on any key press
    }
    
    // Acquire next image
    // Record command buffer
    // Submit to queue
    // Present swapchain
}
```

### 5.3 Cleanup
- Destroy in reverse order of creation
- Use GLFW's cleanup for window

## 6. Configuration (BuildConfig.h)

The application will check `VULKAN_SDK` macro:
- If VULKAN_SDK == 1: Build with Vulkan support
- If VULKAN_SDK == 0: Show error and exit (Vulkan required for this app)

## 7. Future Enhancements (Out of Scope)

- Dynamic shader loading
- Multiple render passes
- Camera/transform support
- Texture mapping
- 3D model loading
- Render to texture

## 8. Estimated Complexity

- Lines of code: ~300-500
- Implementation time: 2-4 hours
- Testing: Basic compilation and runtime verification

================================================================================
                              APPROVAL REQUIRED
================================================================================

Please confirm if this specification is acceptable before implementation.