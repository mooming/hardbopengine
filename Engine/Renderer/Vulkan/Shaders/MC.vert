#version 450
// Vertex shader: transforms a voxel-surface vertex and forwards the normal.
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

layout(push_constant) uniform Push { mat4 view; mat4 proj; } uMat;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vWorldPos;

void main() {
    vWorldPos = aPosition;
    vNormal = aNormal;
    gl_Position = uMat.proj * uMat.view * vec4(aPosition, 1.0);
}
