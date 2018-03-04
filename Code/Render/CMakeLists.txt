cmake_minimum_required (VERSION 2.6)
project (Render)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${ CMAKE_CXX_FLAGS } -Wall -Werror")
endif(CMAKE_COMPILER_IS_GNUCXX)

include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 )

link_directories (${CMAKE_SOURCE_DIR}/lib)



add_library (Render STATIC 
 Renderable.cpp
 Renderer.cpp
 MeshData.h
 Renderable.h
 Renderer.h
 RenderTarget.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Render DESTINATION ${CMAKE_SOURCE_DIR}/lib)


target_link_libraries (Render System Math)

add_dependencies (Render System)
add_dependencies (Render Math)


