cmake_minimum_required (VERSION 3.12)
project (Resource)

set (CMAKE_CXX_STANDARD 23)

if (MSVC)
	add_compile_options (/W3 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ __UNIT_TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/External
 ${CMAKE_SOURCE_DIR}/Engine
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
add_library (Resource STATIC 
 Buffer.cpp
 BufferInputStream.cpp
 BufferOutputStream.cpp
 BufferUtil.cpp
 Resource.cpp
 ResourceManager.cpp
 Buffer.h
 BufferInputStream.h
 BufferOutputStream.h
 BufferTypes.h
 BufferUtil.h
 Resource.h
 ResourceManager.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Resource DESTINATION ${CMAKE_SOURCE_DIR}/lib)


target_link_libraries (Resource OSAL)

add_dependencies (Resource OSAL)


