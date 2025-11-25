cmake_minimum_required (VERSION 3.12)
project (VulkanTest)

set (CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED)

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
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_executable (VulkanTest
 Main.cpp
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS VulkanTest DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (VulkanTest Config Container Core HEngine HSTL Log Math Memory OSAL Resource String Test glfw Vulkan::Vulkan)

add_dependencies (VulkanTest Config)
add_dependencies (VulkanTest Container)
add_dependencies (VulkanTest Core)
add_dependencies (VulkanTest HEngine)
add_dependencies (VulkanTest HSTL)
add_dependencies (VulkanTest Log)
add_dependencies (VulkanTest Math)
add_dependencies (VulkanTest Memory)
add_dependencies (VulkanTest OSAL)
add_dependencies (VulkanTest Resource)
add_dependencies (VulkanTest String)
add_dependencies (VulkanTest Test)
add_dependencies (VulkanTest glfw)



