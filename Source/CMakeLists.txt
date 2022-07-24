cmake_minimum_required (VERSION 2.6)
project (Code)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${ CMAKE_CXX_FLAGS } -Wall -Werror")
endif(CMAKE_COMPILER_IS_GNUCXX)

include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 )

link_directories (${CMAKE_SOURCE_DIR}/lib)


add_subdirectory (Test)
add_subdirectory (Render)
add_subdirectory (Game)
add_subdirectory (Math)
add_subdirectory (System)




