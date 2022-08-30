cmake_minimum_required (VERSION 3.12)
project (Log)

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


add_library (Log STATIC 
 LogUtil.cpp
 Logger.cpp
 LogLevel.h
 LogLine.h
 LogUtil.h
 Logger.h
 PrintArgs.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Log DESTINATION ${CMAKE_SOURCE_DIR}/lib)



