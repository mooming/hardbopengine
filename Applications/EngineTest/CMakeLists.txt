cmake_minimum_required (VERSION 3.12)
project (EngineTest)

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


add_executable (EngineTest
 TestMain.cpp
${PLATFORM_SOURCES}
)

if (APPLE)
	include_directories ( /System/Library/Frameworks )
	set(MACOSX_BUNDLE_TRUE TRUE)
	set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif (APPLE)

install (TARGETS EngineTest DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (EngineTest Config Container Core External HEngine HSTL Log Math Memory OSAL Resource String Test)

add_dependencies (EngineTest Config)
add_dependencies (EngineTest Container)
add_dependencies (EngineTest Core)
add_dependencies (EngineTest External)
add_dependencies (EngineTest HEngine)
add_dependencies (EngineTest HSTL)
add_dependencies (EngineTest Log)
add_dependencies (EngineTest Math)
add_dependencies (EngineTest Memory)
add_dependencies (EngineTest OSAL)
add_dependencies (EngineTest Resource)
add_dependencies (EngineTest String)
add_dependencies (EngineTest Test)


