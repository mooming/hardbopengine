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


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
add_library (Memory STATIC 
 AllocStats.cpp
 AllocStats.cpp
 AllocatorScope.cpp
 AllocatorScope.cpp
 DefaultAllocator.cpp
 DefaultAllocator.cpp
 InlineMonotonicAllocator.cpp
 InlineMonotonicAllocator.cpp
 InlinePoolAllocator.cpp
 InlinePoolAllocator.cpp
 MemoryManager.cpp
 MemoryManager.cpp
 MonotonicAllocator.cpp
 MonotonicAllocator.cpp
 MultiPoolAllocator.cpp
 MultiPoolAllocator.cpp
 MultiPoolAllocatorConfig.cpp
 MultiPoolAllocatorConfig.cpp
 MultiPoolConfigCache.cpp
 MultiPoolConfigCache.cpp
 Optional.cpp
 Optional.cpp
 PoolAllocator.cpp
 PoolAllocator.cpp
 PoolConfigUtil.cpp
 PoolConfigUtil.cpp
 StackAllocator.cpp
 StackAllocator.cpp
 SystemAllocator.cpp
 SystemAllocator.cpp
 ThreadSafeMultiPoolAllocator.cpp
 ThreadSafeMultiPoolAllocator.cpp
 AllocStats.h
 AllocStats.h
 AllocatorID.h
 AllocatorID.h
 AllocatorProxy.h
 AllocatorProxy.h
 AllocatorScope.h
 AllocatorScope.h
 DefaultAllocator.h
 DefaultAllocator.h
 InlineMonotonicAllocator.h
 InlineMonotonicAllocator.h
 InlinePoolAllocator.h
 InlinePoolAllocator.h
 Memory.h
 Memory.h
 MemoryManager.h
 MemoryManager.h
 MonotonicAllocator.h
 MonotonicAllocator.h
 MultiPoolAllocator.h
 MultiPoolAllocator.h
 MultiPoolAllocatorConfig.h
 MultiPoolAllocatorConfig.h
 MultiPoolConfigCache.h
 MultiPoolConfigCache.h
 Optional.h
 Optional.h
 PoolAllocator.h
 PoolAllocator.h
 PoolConfig.h
 PoolConfig.h
 PoolConfigUtil.h
 PoolConfigUtil.h
 ScopedAllocator.h
 ScopedAllocator.h
 Shareable.h
 Shareable.h
 StackAllocator.h
 StackAllocator.h
 SystemAllocator.h
 SystemAllocator.h
 ThreadSafeMultiPoolAllocator.h
 ThreadSafeMultiPoolAllocator.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Memory DESTINATION ${CMAKE_SOURCE_DIR}/lib)



