# Minimal Vulkan Renderer Design Document

**Location:** `Applications/TriangleExample/`  
**Goal:** Create a simple, data-oriented Vulkan renderer that can be iterated upon and later moved to Engine

---

## Architecture Overview

```
Applications/TriangleExample/
├── MinimalVulkanRenderer.h      # Main renderer class
├── MinimalVulkanRenderer.cpp    # Implementation
├── VulkanComponents.h           # ECS-style components
├── VulkanSystems.h              # Systems that process components
├── VulkanPipeline.h             # Vulkan pipeline management
└── Shaders/                     # GLSL shader files
    ├── vertex.vert
    └── fragment.frag
```

### Core Classes

1. **`VulkanRenderer`** — Main entry point, lifecycle management
2. **`VulkanPipeline`** — Handles Vulkan-specific initialization (device, swapchain, pipelines)
3. **`RenderSystem`** — Processes renderable entities
4. **Component structs** — Tightly-packed data (VertexData, Transform, Material)

---

## Data-Oriented Components

### VertexData (Attribute)
```cpp
struct VertexData {
    alignas(16) float position[3];  // XYZ
    alignas(16) float color[4];     // RGBA
    
    // Optional: static factory for easy creation
    static VertexData Create(float x, float y, float z, 
                            float r, float g, float b, float a);
};
```

### Transform (Component)
```cpp
struct Transform {
    alignas(16) float translation[3];  // XYZ
    alignas(16) float rotation[4];     // Quaternion XYZW
    alignas(16) float scale[3];        // XYZ
    
    // Convenience: identity constructor
    Transform() : translation{0, 0, 0}, rotation{0, 0, 0, 1}, scale{1, 1, 1} {}
};
```

### Material (Component)
```cpp
struct Material {
    uint32_t shaderId;      // Reference to shader program
    uint32_t textureId;     // Reference to texture (0 = none)
    
    // Uniform data (example: model matrix stored here)
    alignas(16) float modelMatrix[16];  // 4x4 column-major
    
    Material() : shaderId(0), textureId(0) {}
};
```

### Entity (ID)
```cpp
using EntityId = uint32_t;

struct Entity {
    EntityId id;
    bool isActive;
    
    Entity() : id(0), isActive(true) {}
};
```

---

## Systems

### RenderSystem
Processes all entities with `VertexData`, `Transform`, and `Material` components.

```cpp
class RenderSystem {
public:
    void Update(const std::vector<Entity>& entities,
                const std::vector<VertexData>& vertexData,
                const std::vector<Transform>& transforms,
                const std::vector<Material>& materials);
    
    void Render(VulkanPipeline& pipeline);
};
```

**职责：**
1. Collect all active entities
2. Transform vertices (apply matrix math)
3. Upload to GPU buffers
4. Issue draw calls

---

## Vulkan Pipeline Management

### VulkanPipeline Class
Handles all Vulkan initialization and lifecycle.

```cpp
class VulkanPipeline {
public:
    bool Initialize(OS::IWindow* window);
    void Shutdown();
    
    bool BeginFrame();
    void EndFrame();
    
    // Buffer management
    void UpdateVertexBuffer(const void* data, size_t size);
    void UpdateUniformBuffer(const void* data, size_t size);
    
    // Draw commands
    void Draw(const std::vector<DrawCommand>& commands);

private:
    // Vulkan handles (opaque to user)
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    
    // Buffers
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    
    // Frame synchronization
    uint32_t currentFrame;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
};
```

---

## Implementation Steps

### Step 1: Basic Structure (30 minutes)
**Files:** `MinimalVulkanRenderer.h`, `MinimalVulkanRenderer.cpp`

```cpp
// MinimalVulkanRenderer.h
class MinimalVulkanRenderer {
public:
    bool Initialize(OS::IWindow* window) noexcept;
    void Shutdown() noexcept;
    void BeginFrame() noexcept;
    void EndFrame() noexcept;
    void Render(float deltaTime) noexcept;

private:
    VulkanPipeline pipeline;
    RenderSystem renderSystem;
};
```

**Task:** Create the skeleton, verify it compiles.

---

### Step 2: Vulkan Initialization (2 hours)
**Files:** `VulkanPipeline.h/cpp`

**Sub-tasks:**
1. Create VkInstance with validation layers (if available)
2. Create VkSurfaceKHR from OSAL window
3. Pick physical device (GPU)
4. Create logical device with graphics queue
5. Create swapchain
6. Create render pass
7. Create graphics pipeline (simple vertex/fragment shader)
8. Create vertex buffer with triangle data
9. Create uniform buffer

**Validation:** `vkCreateInstance`, `vkCreateDevice`, `vkCreateSwapchainKHR` all return `VK_SUCCESS`

---

### Step 3: Frame Loop (1 hour)
**Files:** `VulkanPipeline.h/cpp`, `MinimalVulkanRenderer.cpp`

**Sub-tasks:**
1. `BeginFrame()` — Acquire next swapchain image, begin command buffer
2. Record draw commands (vertex buffer, uniform buffer)
3. `EndFrame()` — End command buffer, present to screen
4. Handle frame synchronization (semaphores, fences)

**Validation:** Triangle appears on screen

---

### Step 4: Transform System (1 hour)
**Files:** `VulkanComponents.h`, `VulkanSystems.h`

**Sub-tasks:**
1. Implement Transform component with identity constructor
2. Implement matrix multiplication utilities (vec3 × mat4)
3. Update RenderSystem to apply transforms to vertices
4. Upload transformed vertices to GPU each frame

**Validation:** Triangle rotates/moves/scales

---

### Step 5: Multiple Entities (1 hour)
**Files:** `VulkanComponents.h`, `VulkanSystems.h`, `MinimalVulkanRenderer.cpp`

**Sub-tasks:**
1. Add Entity IDs
2. Create multiple triangles with different transforms
3. Update RenderSystem to process all entities
4. Batch draw calls

**Validation:** Multiple triangles rendered

---

### Step 6: Material System (1 hour)
**Files:** `VulkanComponents.h`, `VulkanSystems.h`

**Sub-tasks:**
1. Add Material component with shader ID
2. Support multiple shader programs
3. Update pipeline to handle different materials

**Validation:** Different colored triangles using different shaders

---

## Vulkan Initialization Checklist

Use this as a reference when implementing `VulkanPipeline::Initialize()`:

- [ ] `vkCreateInstance` — Create Vulkan instance
- [ ] `vkCreateDebugUtilsMessengerEXT` — Enable validation (optional)
- [ ] `vkCreateSurfaceKHR` — Create surface from OSAL window
- [ ] `vkEnumeratePhysicalDevices` — List available GPUs
- [ ] `vkGetPhysicalDeviceProperties` — Pick best GPU
- [ ] `vkCreateDevice` — Create logical device
- [ ] `vkGetDeviceQueue` — Get graphics queue
- [ ] `vkCreateSwapchainKHR` — Create swapchain
- [ ] `vkGetSwapchainImagesKHR` — Get swapchain images
- [ ] `vkCreateImageView` — Create image views for swapchain
- [ ] `vkCreateRenderPass` — Create render pass
- [ ] `vkCreateGraphicsPipelines` — Create graphics pipeline
- [ ] `vkCreateFramebuffer` — Create framebuffers
- [ ] `vkCreateCommandPool` — Create command pool
- [ ] `vkCreateCommandBuffer` — Allocate command buffers
- [ ] `vkCreateBuffer` — Create vertex buffer
- [ ] `vkAllocateMemory` — Allocate memory for vertex buffer
- [ ] `vkBindBufferMemory` — Bind memory to vertex buffer
- [ ] `vkCreateBuffer` — Create uniform buffer
- [ ] `vkAllocateMemory` — Allocate memory for uniform buffer
- [ ] `vkBindBufferMemory` — Bind memory to uniform buffer
- [ ] `vkCreateSemaphore` — Create sync semaphores (x3 for frame count)
- [ ] `vkCreateFence` — Create in-flight fences (x3 for frame count)

---

## Shader Files

### vertex.vert
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

### fragment.frag
```glsl
#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor;
}
```

---

## Testing Strategy

### Test 1: Triangle Appears
- **Expected:** Red triangle on black background
- **Verification:** Run application, see triangle

### Test 2: Triangle Rotates
- **Expected:** Triangle rotates smoothly
- **Verification:** Observe rotation, check FPS

### Test 3: Multiple Triangles
- **Expected:** 3 triangles with different colors/positions
- **Verification:** See all triangles rendered

### Test 4: Memory Leaks
- **Expected:** No leaks on Shutdown
- **Verification:** Run with valgrind or RenderDoc

---

## Common Pitfalls

1. **Missing `vkDeviceWaitIdle()` before Shutdown** — Cleanup must wait for all GPU work to finish
2. **Forgetting to call `vkQueuePresentKHR`** — Nothing will appear on screen
3. **Not handling swapchain recreation** — Window resize requires swapchain rebuild
4. **Incorrect vertex format** — Must match pipeline input assembly
5. **Forgetting to upload uniform data** — Transform won't apply
6. **Memory leaks on Vulkan handles** — Every `vkCreate*` needs corresponding `vkDestroy*`

---

## File Structure for Implementation

```
Applications/TriangleExample/
├── Main.cpp                          # Existing, minimal changes
├── MinimalVulkanRenderer.h           # NEW: Renderer class declaration
├── MinimalVulkanRenderer.cpp         # NEW: Renderer implementation
├── VulkanComponents.h                # NEW: Component structs
├── VulkanSystems.h                   # NEW: System classes
├── VulkanPipeline.h                  # NEW: Vulkan initialization
├── VulkanPipeline.cpp                # NEW: Vulkan implementation
├── Shaders/
│   ├── vertex.vert                   # NEW: Vertex shader
│   └── fragment.frag                 # NEW: Fragment shader
└── CMakeLists.txt                    # Update: Add new source files
```

---

## CMakeLists.txt Updates

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

# Add shader copy step (optional)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Shaders DESTINATION ${CMAKE_BINARY_DIR})
```

---

## Next Steps

1. **Start with Step 1** — Create the skeleton, verify compilation
2. **Move to Step 2** — Implement Vulkan initialization (this is the biggest chunk)
3. **Iterate** — Each step should be buildable and testable

**Tip:** Use RenderDoc or Vulkan Validation Layers for debugging. Enable validation layers early to catch errors.

---

## Resources

- [Vulkan Tutorial](https://vulkan-tutorial.com/) — Excellent step-by-step guide
- [Vulkan Samples](https://github.com/KhronosGroup/Vulkan-Samples) — Official samples
- [Learn Vulkan](https://vulkan.learnml.com/) — Community resource
- [Vulkan SDK](https://vulkan.lunarg.com/) — Download with tools (glslc, spirv-cross, etc.)

---

**Document Version:** 1.0  
**Last Updated:** 2026-07-14  
**Author:** Design document for user implementation
