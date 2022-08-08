cmake_minimum_required (VERSION 3.12)
project (Memory)

set (CMAKE_CXX_STANDARD 20)

if (MSVC)
	add_compile_options (/W4 /WX)
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
 AllocatorScope.cpp
 BaseAllocator.cpp
 InlineAllocator.cpp
 InlinePoolAllocator.cpp
 MemoryManager.cpp
 Optional.cpp
 PoolAllocator.cpp
 StackAllocator.cpp
 SystemAllocator.cpp
 AllocatorID.h
 AllocatorScope.h
 BaseAllocator.h
 InlineAllocator.h
 InlinePoolAllocator.h
 Memory.h
 MemoryManager.h
 Optional.h
 PoolAllocator.h
 Shareable.h
 StackAllocator.h
 SystemAllocator.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Memory DESTINATION ${CMAKE_SOURCE_DIR}/lib)



