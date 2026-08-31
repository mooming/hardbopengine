#version 450
// Fragment shader: one directional light + Lambert shading.
layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vWorldPos;

void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.5));
    float diff = max(dot(n, lightDir), 0.0);
    vec3 ambient = vec3(0.25);
    vec3 color = ambient + diff * vec3(0.95);
    // simple distance fade so distant terrain reads as depth
    float depth = clamp(vWorldPos.y * 0.0 + gl_FragCoord.z, 0.0, 1.0);
    outColor = vec4(color, 1.0);
}
