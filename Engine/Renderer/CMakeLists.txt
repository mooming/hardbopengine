cmake_minimum_required (VERSION 3.12)
project (Renderer)

set (CMAKE_CONFIGURATION_TYPES "Debug;Dev;Release" CACHE STRING "" FORCE)
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif ()

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set (CMAKE_CXX_FLAGS_DEV "${CMAKE_CXX_FLAGS_DEBUG} -O1")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

set (CMAKE_CXX_STANDARD 23)

if (APPLE)
    set(PLATFORM_SOURCES
    )
else()
    set(PLATFORM_SOURCES "")
endif()

if (MSVC)
	add_compile_options (/W3 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/External
 ${CMAKE_SOURCE_DIR}/Engine
 ${CMAKE_SOURCE_DIR}/Engine/Renderer
 )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
add_library (Renderer STATIC 
 RHICapabilities.cpp
 RendererTest.cpp
 RHICapabilities.h
 RendererCommon.h
 RendererTest.h
${PLATFORM_SOURCES}
)

target_include_directories (Renderer PUBLIC
 Engine/Renderer
)

if (APPLE)
	include_directories ( /System/Library/Frameworks )
	set(MACOSX_BUNDLE_TRUE TRUE)
	set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif (APPLE)

install (TARGETS Renderer DESTINATION ${CMAKE_SOURCE_DIR}/lib)


target_link_libraries (Renderer Log)

add_dependencies (Renderer Log)


# Custom build configuration for the Renderer module.
# Keeps the Vulkan source wiring, headers and Apple framework linking self-contained
# so the generated CMakeLists.txt never needs hand-editing.
#
# NOTE: MakeBuild's ParseList() de-duplicates identical lines, so every endif
# here carries a unique comment suffix to survive de-duplication intact.
# Engine/Renderer/.module.config lists Vulkan in ignoreSubdirectories, so MakeBuild
# cannot emit these sources into the target - wire them in explicitly.
target_sources (Renderer PRIVATE Vulkan/VulkanRenderer.cpp)
# Locate the Vulkan headers via common macOS / homebrew / standard locations.
find_path (VULKAN_INCLUDE_DIR vulkan/vulkan.h PATHS /opt/homebrew/include /usr/local/include /usr/include $ENV{VULKAN_SDK}/include)
if (NOT VULKAN_INCLUDE_DIR)
    message (FATAL_ERROR "vulkan/vulkan.h not found. Install the Vulkan headers, or set VULKAN_SDK.")
endif () # vulkan headers
target_include_directories (Renderer PUBLIC ${VULKAN_INCLUDE_DIR})
# The engine is Vulkan-only, so a missing loader is a hard error, not a silent skip.
# NOTE: plain (non-keyword) signature throughout - MakeBuild emits the plain form for
# module dependencies, and CMake forbids mixing the two for one target.
find_library (VULKAN_LOADER_LIB NAMES vulkan PATHS /opt/homebrew/lib /usr/local/lib /usr/lib $ENV{VULKAN_SDK}/lib)
if (NOT VULKAN_LOADER_LIB)
    message (FATAL_ERROR "Vulkan loader (libvulkan) not found. Install it, or set VULKAN_SDK.")
endif () # vulkan loader
target_link_libraries (Renderer ${VULKAN_LOADER_LIB})
# macOS: the surface lives in an Objective-C++ unit and needs the Metal-backed
# surface declarations, which vulkan_metal.h only exposes behind this define.
if (APPLE)
    target_sources (Renderer PRIVATE Vulkan/VulkanRenderer.mm)
    target_compile_definitions (Renderer PUBLIC VK_USE_PLATFORM_METAL_EXT)
    find_library (COCOA_LIB NAMES Cocoa)
    find_library (METAL_LIB NAMES Metal)
    find_library (QUARTZCORE_LIB NAMES QuartzCore)
    if (COCOA_LIB)
        target_link_libraries (Renderer ${COCOA_LIB})
    endif () # cocoa
    if (METAL_LIB)
        target_link_libraries (Renderer ${METAL_LIB})
    endif () # metal
    if (QUARTZCORE_LIB)
        target_link_libraries (Renderer ${QUARTZCORE_LIB})
    endif () # quartzcore
endif () # apple

