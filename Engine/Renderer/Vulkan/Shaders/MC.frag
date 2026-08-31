#version 450
// Fragment shader: one directional light + Lambert shading.
//
// Regenerate with:
//   glslangValidator -V MC.frag -o MC.frag.spv
//   python3 ../gen_spv_header.py MC.vert.spv MC.frag.spv ../ShadersSpv.h

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 vNormal;

// Directional light is still compile-time fixed: model + viewProj already consume
// the whole 128-byte push range. It becomes app-controlled when uniforms move to a
// descriptor-buffer uniform buffer (see PLAN_real_vulkan_renderer.md).
const vec3 kLightDir = normalize(vec3(0.4, 1.0, 0.5));
const vec3 kAmbient = vec3(0.25);
const vec3 kAlbedo = vec3(0.95);

void main() {
    vec3 n = normalize(vNormal);
    float diff = max(dot(n, kLightDir), 0.0);
    outColor = vec4(kAmbient + diff * kAlbedo, 1.0);
}
