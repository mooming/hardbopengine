cmake_minimum_required (VERSION 3.12)
project (Container)

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
 ${CMAKE_SOURCE_DIR}/External
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
add_library (Container STATIC 
 Array.cpp
 AtomicStackView.cpp
 LinkedList.cpp
 Array.h
 AtomicStackView.h
 LinkedList.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Container DESTINATION ${CMAKE_SOURCE_DIR}/lib)



