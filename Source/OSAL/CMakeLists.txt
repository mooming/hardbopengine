cmake_minimum_required (VERSION 3.12)
project (OSAL)

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
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_library (OSAL STATIC 
 Directory.cpp
 OSAbstractLayer.cpp
 OSMemory.cpp
 OSThread.cpp
 Directory.h
 File.h
 Intrinsic.h
 OSAbstractLayer.h
 OSMemory.h
 OSThread.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS OSAL DESTINATION ${CMAKE_SOURCE_DIR}/lib)



