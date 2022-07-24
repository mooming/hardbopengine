cmake_minimum_required (VERSION 2.6)
project (System)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${ CMAKE_CXX_FLAGS } -Wall -Werror")
endif(CMAKE_COMPILER_IS_GNUCXX)
add_definitions (-std=c++14 -D__UNIT_TEST__ )

include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 )

link_directories (${CMAKE_SOURCE_DIR}/lib)



add_library (System STATIC 
 TestEnv.cpp
 TestCase.cpp
 LinkedList.cpp
 StdUtil.cpp
 Component.cpp
 Optional.cpp
 HardbopAllocator.cpp
 String.cpp
 Array.cpp
 Vector.cpp
 Allocator.cpp
 MemoryManager.cpp
 CommonUtil.cpp
 StackAllocator.cpp
 PoolAllocator.cpp
 ComponentSystem.cpp
 Time.cpp
 Time.h
 HardbopAllocator.h
 Debug.h
 StackAllocator.h
 Shareable.h
 Types.h
 StdUtil.h
 DoubleLinkedList.h
 MemoryManager.h
 Letter.h
 LinkedList.h
 Optional.h
 ComponentState.h
 PoolAllocator.h
 TestCase.h
 Constants.h
 TestEnv.h
 Allocator.h
 Array.h
 ComponentSystem.h
 Component.h
 MathUtil.h
 Vector.h
 Exception.h
 PrintArgs.h
 Platform.h
 String.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS System DESTINATION ${CMAKE_SOURCE_DIR}/lib)



