cmake_minimum_required (VERSION 3.12)
project (WindowExample)

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


add_executable (WindowExample
 Main.cpp
${PLATFORM_SOURCES}
)

if (APPLE)
	include_directories ( /System/Library/Frameworks )
	set(MACOSX_BUNDLE_TRUE TRUE)
	set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif (APPLE)

install (TARGETS WindowExample DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (WindowExample Config Container Core HEngine HSTL Log Math Memory OSAL Resource String Test)

add_dependencies (WindowExample Config)
add_dependencies (WindowExample Container)
add_dependencies (WindowExample Core)
add_dependencies (WindowExample HEngine)
add_dependencies (WindowExample HSTL)
add_dependencies (WindowExample Log)
add_dependencies (WindowExample Math)
add_dependencies (WindowExample Memory)
add_dependencies (WindowExample OSAL)
add_dependencies (WindowExample Resource)
add_dependencies (WindowExample String)
add_dependencies (WindowExample Test)


