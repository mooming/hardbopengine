cmake_minimum_required (VERSION 2.6)
project (Game)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${ CMAKE_CXX_FLAGS } -Wall -Werror")
endif(CMAKE_COMPILER_IS_GNUCXX)

include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 )

link_directories (${CMAKE_SOURCE_DIR}/lib)



add_library (Game STATIC 
 GameEngine.cpp
 GameObject.cpp
 GameScene.cpp
 GameEngine.h
 GameObject.h
 GameScene.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Game DESTINATION ${CMAKE_SOURCE_DIR}/lib)


target_link_libraries (Game System Math Render)

add_dependencies (Game System)
add_dependencies (Game Math)
add_dependencies (Game Render)


