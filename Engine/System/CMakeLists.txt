cmake_minimum_required (VERSION 3.12)
project (System)

set (CMAKE_CXX_STANDARD 17)

if (MSVC)
	add_compile_options (/W4 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/Engine
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_library (System STATIC 
 Allocator.cpp
 Array.cpp
 CommonUtil.cpp
 Component.cpp
 ComponentSystem.cpp
 HardbopAllocator.cpp
 LinkedList.cpp
 MemoryManager.cpp
 Optional.cpp
 PoolAllocator.cpp
 StackAllocator.cpp
 StdUtil.cpp
 String.cpp
 TestCase.cpp
 TestEnv.cpp
 Time.cpp
 Vector.cpp
 Allocator.h
 Array.h
 Component.h
 ComponentState.h
 ComponentSystem.h
 Constants.h
 Debug.h
 DoubleLinkedList.h
 Exception.h
 HardbopAllocator.h
 Letter.h
 LinkedList.h
 MathUtil.h
 MemoryManager.h
 Optional.h
 Platform.h
 PoolAllocator.h
 PrintArgs.h
 Shareable.h
 StackAllocator.h
 StdUtil.h
 String.h
 TestCase.h
 TestEnv.h
 Time.h
 Types.h
 Vector.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS System DESTINATION ${CMAKE_SOURCE_DIR}/lib)



