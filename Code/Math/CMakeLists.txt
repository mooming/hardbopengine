cmake_minimum_required (VERSION 2.6)
project (Math)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${ CMAKE_CXX_FLAGS } -Wall -Werror")
endif(CMAKE_COMPILER_IS_GNUCXX)
add_definitions (-std=c++14 -D__UNIT_TEST__ )

include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 )

link_directories (${CMAKE_SOURCE_DIR}/lib)



add_library (Math STATIC 
 Vector4.cpp
 Vector3.cpp
 Matrix4x4.cpp
 Matrix2x2.cpp
 Vector2.cpp
 OBB.cpp
 AABB.cpp
 Matrix3x3.cpp
 RigidTransform.cpp
 Transform.cpp
 Quaternion.cpp
 UniformTransform.cpp
 Matrix4x4.h
 Vector2.h
 CoordinateOrientation.h
 Matrix3x3.h
 Quaternion.h
 MatrixCommonImpl.inl
 VectorCommonImpl.inl
 Matrix2x2.h
 RigidTransform.h
 Vector4.h
 Transform.h
 UniformTransform.h
 OBB.h
 AABB.h
 Vector3.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Math DESTINATION ${CMAKE_SOURCE_DIR}/lib)



