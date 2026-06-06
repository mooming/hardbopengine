cmake_minimum_required (VERSION 3.12)
project (TriangleExample)

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
 ${CMAKE_SOURCE_DIR}/Applications/TriangleExample
 ${CMAKE_SOURCE_DIR}/Applications/VulkanExample
 ${CMAKE_SOURCE_DIR}/External
 ${CMAKE_SOURCE_DIR}/Engine
 ${CMAKE_SOURCE_DIR}/Engine/Renderer
 ${CMAKE_SOURCE_DIR}/Applications/TriangleExample/../External/VulkanSDK/include
 )
link_directories (${CMAKE_SOURCE_DIR}/lib
)


add_executable (TriangleExample
 Main.cpp
 SoftwareRenderer.cpp
 TriangleRenderTest.cpp
 DODTypes.h
 SoftwareRenderer.h
${PLATFORM_SOURCES}
)

if (APPLE)
	include_directories ( /System/Library/Frameworks )
	set(MACOSX_BUNDLE_TRUE TRUE)
	set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif (APPLE)

install (TARGETS TriangleExample DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (TriangleExample Config Container Core HSTL Log Math Memory OSAL Resource String HEngine Test Renderer)

add_dependencies (TriangleExample Config)
add_dependencies (TriangleExample Container)
add_dependencies (TriangleExample Core)
add_dependencies (TriangleExample HSTL)
add_dependencies (TriangleExample Log)
add_dependencies (TriangleExample Math)
add_dependencies (TriangleExample Memory)
add_dependencies (TriangleExample OSAL)
add_dependencies (TriangleExample Resource)
add_dependencies (TriangleExample String)
add_dependencies (TriangleExample HEngine)
add_dependencies (TriangleExample Test)
add_dependencies (TriangleExample Renderer)


add_definitions(-D__TEST__ -D__UNIT_TEST__)
include_directories(${CMAKE_SOURCE_DIR}/External/VulkanSDK/include)
