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
 Allocator.cpp
 Array.cpp
 CommonUtil.cpp
 Component.cpp
 ComponentSystem.cpp
 LinkedList.cpp
 MemoryManager.cpp
 Optional.cpp
 PoolAllocator.cpp
 StackAllocator.cpp
 String.cpp
 TestCase.cpp
 TestEnv.cpp
 Time.cpp
 Vector.cpp
 Allocator.h
 Array.h
 CommonUtil.h
 Component.h
 ComponentState.h
 ComponentSystem.h
 Constants.h
 Debug.h
 DoubleLinkedList.h
 Exception.h
 Letter.h
 LinkedList.h
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



