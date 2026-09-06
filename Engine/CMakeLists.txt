cmake_minimum_required (VERSION 3.12)
project (HEngineBase)

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
add_subdirectory (Config)
add_subdirectory (Container)
add_subdirectory (Core)
add_subdirectory (Engine)
add_subdirectory (HSTL)
add_subdirectory (Log)
add_subdirectory (Math)
add_subdirectory (Memory)
add_subdirectory (OSAL)
add_subdirectory (Renderer)
add_subdirectory (Resource)
add_subdirectory (String)
add_subdirectory (Test)

# CodingStandards.{h,cpp} is the executable statement of this project's style; every
# other file derives its rules from it. It belonged to no target, so it silently
# drifted out of agreement with the standard it documents — 4-space indentation in a
# tabs-only tree, a brace rule described backwards, and an include layout that its
# own formatter rejects. Compiling it here makes that drift loud instead of quiet:
# it is built with the same -Wall -Werror flags as the engine, in Debug, Dev and
# Release.
#
# Deliberately NOT linked into HEngine. This is a documentation artefact, not runtime
# code, and it must not reach a shipping binary; it is therefore neither installed
# into lib/ the way the sibling modules are, nor redirected there via
# CMAKE_ARCHIVE_OUTPUT_DIRECTORY — the .a stays inside the build tree.
add_library (CodingStandards STATIC
 CodingStandards.cpp
 CodingStandards.h
${PLATFORM_SOURCES}
)




