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
 AABB.cpp
 AffineTransform.cpp
 EuclidSpace.cpp
 Matrix2x2.cpp
 Matrix3x3.cpp
 Matrix4x4.cpp
 Quaternion.cpp
 RigidTransform.cpp
 UniformTransform.cpp
 Vector2.cpp
 Vector3.cpp
 Vector4.cpp
 AABB.h
 AffineTransform.h
 CoordinateOrientation.h
 EuclidSpace.h
 Matrix2x2.h
 Matrix3x3.h
 Matrix4x4.h
 MatrixCommonImpl.inl
 Quaternion.h
 RigidTransform.h
 UniformTransform.h
 Vector2.h
 Vector3.h
 Vector4.h
 VectorCommonImpl.inl
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Math DESTINATION ${CMAKE_SOURCE_DIR}/lib)



