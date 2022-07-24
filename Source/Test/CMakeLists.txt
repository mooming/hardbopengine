cmake_minimum_required (VERSION 2.6)
project (Test)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${ CMAKE_CXX_FLAGS } -Wall -Werror")
endif(CMAKE_COMPILER_IS_GNUCXX)
add_definitions (-std=c++14 -D__UNIT_TEST__ )

include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 )

link_directories (${CMAKE_SOURCE_DIR}/lib)



add_executable (Test
 TestMain.cpp
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Test DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (Test System Math)

add_dependencies (Test System)
add_dependencies (Test Math)


