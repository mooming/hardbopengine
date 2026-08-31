#version 450
// Vertex shader: transforms a voxel-surface vertex and forwards the world normal.
//
// Regenerate with:
//   glslangValidator -V MC.vert -o MC.vert.spv
//   glslangValidator -V MC.frag -o MC.frag.spv
//   python3 ../gen_spv_header.py MC.vert.spv MC.frag.spv ../ShadersSpv.h

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

// model + viewProj = 2 x mat4 = 128 bytes, the portability-guaranteed
// maxPushConstantsSize. viewProj is combined on the CPU once per frame.
layout(push_constant) uniform Push { mat4 model; mat4 viewProj; } uMat;

layout(location = 0) out vec3 vNormal;

void main() {
    vec4 worldPos = uMat.model * vec4(aPosition, 1.0);

    // Valid while model carries rotation/translation only (no non-uniform scale);
    // switch to transpose(inverse(model)) the moment scaling is introduced.
    vNormal = mat3(uMat.model) * aNormal;

    gl_Position = uMat.viewProj * worldPos;
}
