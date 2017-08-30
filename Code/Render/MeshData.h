// Copyright, All right reserved by Hansol Park, mooming.go@gmail.com

#ifndef MESHDATA_H
#define MESHDATA_H

#include "../System/Vector.h"

#include "../Math/AABB.h"
#include "../Math/Vector2.h"
#include "../Math/Vector3.h"

namespace HE
{

  class MeshData
  {
    template<typename T>
    using vector<T> = Vector<T>;

  public:
    vector<int> indices;
    vector<Float3> vertices;
    vector<Float3> normals;
    vector<Float2> uvs;
    vector<Float2> uvs2;

    AABB3 boundBox;

  public:

    inline MeshData() : indices(), vertices(), normals(), uvs(), uvs2(), boundBox()
    {
    }

    inline void CalculateBoundBox()
    {
      boundBox.Reset();

      for (const auto& vertex : vertices)
      {
        boundBox.Add(vertex);
      }
    }

    inline void CalculateNormals()
    {
      auto length = indices.Size();
      decltype(length) index = 0;

      for (decltype(length) i = 0; i < length; i += 3)
      {
        const auto& o = vertices[indices[i]];
        const auto& a = vertices[indices[i + 1]];
        const auto& b = vertices[indices[i + 2]];

        normals[index++] = (a - o).Cross(b - o).Normalized();
      }
    }

  };
}

#endif  // MESHDATA_H

