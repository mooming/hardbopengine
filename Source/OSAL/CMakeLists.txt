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
 ${CMAKE_SOURCE_DIR}/External
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
add_library (OSAL STATIC 
 Directory.cpp
 LinuxAbstractLayer.cpp
 LinuxDebug.cpp
 LinuxFileHandle.cpp
 LinuxFileOpenMode.cpp
 LinuxInputOutput.cpp
 LinuxMapSyncMode.cpp
 LinuxMemory.cpp
 LinuxProtectionMode.cpp
 LinuxThread.cpp
 OSAbstractLayer.cpp
 OSDebug.cpp
 OSInputOutput.cpp
 OSMemory.cpp
 OSThread.cpp
 OSXAbstractLayer.cpp
 OSXDebug.cpp
 OSXFileHandle.cpp
 OSXFileOpenMode.cpp
 OSXInputOutput.cpp
 OSXMapSyncMode.cpp
 OSXMemory.cpp
 OSXProtectionMode.cpp
 OSXThread.cpp
 WindowsAbstractLayer.cpp
 WindowsDebug.cpp
 WindowsInputOutput.cpp
 WindowsMemory.cpp
 WindowsThread.cpp
 Directory.h
 File.h
 Intrinsic.h
 LinuxIntrinsic.h
 OSAbstractLayer.h
 OSDebug.h
 OSFileHandle.h
 OSFileOpenMode.h
 OSInputOutput.h
 OSMapSyncMode.h
 OSMemory.h
 OSProtectionMode.h
 OSThread.h
 OSXIntrinsic.h
 SourceLocation.h
 WindowsIntrinsic.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS OSAL DESTINATION ${CMAKE_SOURCE_DIR}/lib)



