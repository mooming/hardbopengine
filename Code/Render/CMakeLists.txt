cmake_minimum_required (VERSION 2.6)
project (Render)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${ CMAKE_CXX_FLAGS } -Wall -Werror")
endif(CMAKE_COMPILER_IS_GNUCXX)

include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 )



add_library (Render INTERFACE)
target_sources (Render INTERFACE 
 MeshData.h
)


target_link_libraries (Render System Math)

add_dependencies (Render System)
add_dependencies (Render Math)


