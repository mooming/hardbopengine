cmake_minimum_required (VERSION 3.12)
project (VulkanExample)

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

add_compile_definitions (__TEST__ __UNIT_TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/External
 ${CMAKE_SOURCE_DIR}/Engine
 )
link_directories (${CMAKE_SOURCE_DIR}/lib
 )


add_executable (VulkanExample
  Main.cpp
 ${PLATFORM_SOURCES}
)

if (APPLE)
    include_directories ( /System/Library/Frameworks )
    set(MACOSX_BUNDLE_TRUE TRUE)
    set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif (APPLE)

install (TARGETS VulkanExample DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (VulkanExample Config Container Core HEngine HSTL Log Math Memory OSAL Renderer VulkanRenderer MetalRenderer Resource String Test)

add_dependencies (VulkanExample Config)
add_dependencies (VulkanExample Container)
add_dependencies (VulkanExample Core)
add_dependencies (VulkanExample HEngine)
add_dependencies (VulkanExample HSTL)
add_dependencies (VulkanExample Log)
add_dependencies (VulkanExample Math)
add_dependencies (VulkanExample Memory)
add_dependencies (VulkanExample OSAL)
add_dependencies (VulkanExample Renderer)
add_dependencies (VulkanExample Resource)
add_dependencies (VulkanExample String)
add_dependencies (VulkanExample Test)