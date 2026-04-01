cmake_minimum_required (VERSION 3.12)
project (VulkanTriangle)

set (CMAKE_CONFIGURATION_TYPES "Debug;Dev;Release" CACHE STRING "" FORCE)
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif ()

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set (CMAKE_CXX_FLAGS_DEV "${CMAKE_CXX_FLAGS_DEBUG} -O1")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

set (CMAKE_CXX_STANDARD 23)

if (MSVC)
	add_compile_options (/W3 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ __UNIT_TEST__ GLFW_INCLUDE_VULKAN)
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/External
 ${CMAKE_SOURCE_DIR}/Engine
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_executable (VulkanTriangle
 VulkanTriangle.cpp
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS VulkanTriangle DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (VulkanTriangle Config Container Core HEngine HSTL Log Math Memory OSAL Resource String glfw vulkan)

add_dependencies (VulkanTriangle Config)
add_dependencies (VulkanTriangle Container)
add_dependencies (VulkanTriangle Core)
add_dependencies (VulkanTriangle HEngine)
add_dependencies (VulkanTriangle HSTL)
add_dependencies (VulkanTriangle Log)
add_dependencies (VulkanTriangle Math)
add_dependencies (VulkanTriangle Memory)
add_dependencies (VulkanTriangle OSAL)
add_dependencies (VulkanTriangle Resource)
add_dependencies (VulkanTriangle String)
add_dependencies (VulkanTriangle glfw)