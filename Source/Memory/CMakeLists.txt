cmake_minimum_required (VERSION 3.12)
project (Memory)

set (CMAKE_CXX_STANDARD 20)

if (MSVC)
	add_compile_options (/W3 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ __UNIT_TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/Source
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_library (Memory STATIC 
 AllocStats.cpp
 AllocatorScope.cpp
 BaseAllocator.cpp
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
 AllocStats.h
 AllocatorID.h
 AllocatorProxy.h
 AllocatorScope.h
 BaseAllocator.h
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
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Memory DESTINATION ${CMAKE_SOURCE_DIR}/lib)



