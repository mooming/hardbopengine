cmake_minimum_required (VERSION 3.12)
project (String)

set (CMAKE_CXX_STANDARD 20)

if (MSVC)
	add_compile_options (/W4 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ __UNIT_TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/Source
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_library (String STATIC 
 StaticString.cpp
 StaticStringTable.cpp
 String.cpp
 StringUtil.cpp
 Letter.h
 StaticString.h
 StaticStringTable.h
 String.h
 StringUtil.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS String DESTINATION ${CMAKE_SOURCE_DIR}/lib)



