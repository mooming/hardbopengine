// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 position [[attribute(0)]];
    float4 color [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

vertex VertexOut vertexShader(uint vertexID [[vertex_id]],
                       constant float *vertices [[buffer(0)]]) {
    VertexOut out;
    float2 pos = float2(vertices[vertexID * 6], vertices[vertexID * 6 + 1]);
    out.position = float4(pos, 0.0, 1.0);
    out.color = float4(vertices[vertexID * 6 + 2], vertices[vertexID * 6 + 3],
                     vertices[vertexID * 6 + 4], vertices[vertexID * 6 + 5]);
    return out;
}

fragment float4 fragmentShader(VertexOut in [[stage_in]]) {
    return in.color;
}