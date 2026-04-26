add_library (Renderer STATIC
    RendererCommon.h
    RHICapabilities.h
    RHICapabilities.cpp
    IRenderer.h
    RendererFactory.h
    RendererFactory.cpp
)

target_include_directories (Renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    /opt/homebrew/include
    ${CMAKE_SOURCE_DIR}/External/VulkanSDK/include
)

add_library (Renderer::Renderer ALIAS Renderer)

add_library (VulkanRenderer STATIC
    Vulkan/VulkanRenderer.h
    Vulkan/VulkanRenderer.cpp
    Vulkan/VulkanRenderer.mm
)

target_include_directories (VulkanRenderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    /opt/homebrew/include
    ${CMAKE_SOURCE_DIR}/External/VulkanSDK/include
)

target_link_libraries (VulkanRenderer PRIVATE Renderer)
find_library(VULKAN_LIBRARY vulkan HINT /usr/local/lib /opt/homebrew/lib)
if (VULKAN_LIBRARY)
    target_link_libraries (VulkanRenderer PRIVATE ${VULKAN_LIBRARY})
else()
    message(WARNING "Vulkan library not found, Vulkan rendering will not work")
endif()

add_library (MetalRenderer STATIC
    Metal/MetalRenderer.h
    Metal/MetalRenderer.mm
    Metal/Shaders.metal
)

target_include_directories (MetalRenderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries (MetalRenderer PRIVATE Renderer)
target_link_libraries (MetalRenderer PRIVATE "-framework Metal")
target_link_libraries (MetalRenderer PRIVATE "-framework MetalKit")
target_link_libraries (MetalRenderer PRIVATE "-framework QuartzCore")