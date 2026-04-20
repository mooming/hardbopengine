cmake_minimum_required (VERSION 3.12)
project (Memory)

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

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
add_library (Memory STATIC 
 AllocStats.cpp
 AllocatorScope.cpp
 DefaultAllocator.cpp
 InlineMonotonicAllocator.cpp
 InlinePoolAllocator.cpp
 MemoryManager.cpp
 MonotonicAllocator.cpp
 MultiPoolAllocator.cpp
 MultiPoolAllocatorConfig.cpp
 MultiPoolConfigCache.cpp
 Optional.cpp
 PoolAllocator.cpp
 PoolConfigUtil.cpp
 StackAllocator.cpp
 SystemAllocator.cpp
 ThreadSafeMultiPoolAllocator.cpp
 AllocStats.h
 AllocatorID.h
 AllocatorProxy.h
 AllocatorScope.h
 DefaultAllocator.h
 InlineMonotonicAllocator.h
 InlinePoolAllocator.h
 Memory.h
 MemoryManager.h
 MonotonicAllocator.h
 MultiPoolAllocator.h
 MultiPoolAllocatorConfig.h
 MultiPoolConfigCache.h
 Optional.h
 PoolAllocator.h
 PoolConfig.h
 PoolConfigUtil.h
 ScopedAllocator.h
 Shareable.h
 StackAllocator.h
 SystemAllocator.h
 ThreadSafeMultiPoolAllocator.h
${PLATFORM_SOURCES}
)

if (APPLE)
	include_directories ( /System/Library/Frameworks )
	set(MACOSX_BUNDLE_TRUE TRUE)
	set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif (APPLE)

install (TARGETS Memory DESTINATION ${CMAKE_SOURCE_DIR}/lib)



