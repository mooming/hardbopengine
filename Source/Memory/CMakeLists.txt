cmake_minimum_required (VERSION 3.12)
project (Memory)

set (CMAKE_CXX_STANDARD 17)

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
 Allocator.cpp
 HardbopAllocator.cpp
 MemoryManager.cpp
 Optional.cpp
 PoolAllocator.cpp
 StackAllocator.cpp
 Allocator.h
 BaseAllocator.h
 HardbopAllocator.h
 InlineAllocator.h
 MemoryManager.h
 Optional.h
 PoolAllocator.h
 ScopedAllocator.h
 Shareable.h
 StackAllocator.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Memory DESTINATION ${CMAKE_SOURCE_DIR}/lib)



