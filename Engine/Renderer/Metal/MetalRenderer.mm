// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

// MetalRenderer.mm - Objective-C++ implementation with full render pipeline
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>

#include "MetalRenderer.h"
#include <cmath>

namespace hbe {
namespace Renderer {

struct QuadVertex {
    float x, y;
    float r, g, b, a;
};

MetalRenderer::MetalRenderer()
{
    m_APIType = APIType::Metal;
    m_Capabilities.apiType = APIType::Metal;
    m_Capabilities.supportsComputeShader = true;
    m_Capabilities.maxTextureSize = 4096;
    m_Capabilities.maxVertexAttribs = 31;
}

MetalRenderer::~MetalRenderer()
{
    Shutdown();
}

bool MetalRenderer::Initialize(OS::IWindow* window)
{
    if (m_Initialized)
    {
        return true;
    }

    m_Window = window;

    NSWindow* nsWindow = (NSWindow*)window->GetNativeHandle();
    if (nsWindow == nil)
    {
        return false;
    }

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil)
    {
        return false;
    }
    m_Device = (intptr_t)CFBridgingRetain(device);

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil)
    {
        return false;
    }
    m_CommandQueue = (intptr_t)CFBridgingRetain(queue);

    NSView* contentView = nsWindow.contentView;
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    metalLayer.device = device;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.framebufferOnly = NO;
    metalLayer.drawableSize = CGSizeMake(window->GetWidth(), window->GetHeight());
    metalLayer.contentsScale = nsWindow.backingScaleFactor;

    contentView.wantsLayer = YES;
    contentView.layer = metalLayer;
    contentView.layer.contentsScale = nsWindow.backingScaleFactor;
    m_MetalLayer = (intptr_t)CFBridgingRetain(metalLayer);

    NSString* shaderSource = @""
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "\n"
        "struct VertexOut {\n"
        "    float4 position [[position]];\n"
        "    float4 color;\n"
        "};\n"
        "\n"
        "vertex VertexOut vertexShader(uint vertexID [[vertex_id]],\n"
        "                   constant float *vertices [[buffer(0)]]) {\n"
        "    VertexOut out;\n"
        "    float2 pos = float2(vertices[vertexID * 6], vertices[vertexID * 6 + 1]);\n"
        "    out.position = float4(pos, 0.0, 1.0);\n"
        "    out.color = float4(vertices[vertexID * 6 + 2], vertices[vertexID * 6 + 3],\n"
        "                      vertices[vertexID * 6 + 4], vertices[vertexID * 6 + 5]);\n"
        "    return out;\n"
        "}\n"
        "\n"
        "fragment float4 fragmentShader(VertexOut in [[stage_in]]) {\n"
        "    return in.color;\n"
        "}\n"
        "";

    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    NSError* compileError = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource
                                              options:options
                                                error:&compileError];
    if (library == nil)
    {
        return false;
    }

    id<MTLFunction> vertexFunc = [library newFunctionWithName:@"vertexShader"];
    id<MTLFunction> fragmentFunc = [library newFunctionWithName:@"fragmentShader"];

    if (vertexFunc == nil || fragmentFunc == nil)
    {
        return false;
    }

    MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.label = @"QuadPipeline";
    pipelineDesc.vertexFunction = vertexFunc;
    pipelineDesc.fragmentFunction = fragmentFunc;
    pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    NSError* pipelineError = nil;
    id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&pipelineError];
    if (pipeline == nil)
    {
        return false;
    }

    m_PipelineState = (intptr_t)CFBridgingRetain(pipeline);

    QuadVertex vertices[6] = {
        {-0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 1.0f},
        { 0.5f, -0.5f,  0.0f, 1.0f, 0.0f, 1.0f},
        { 0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f},
        {-0.5f, -0.5f,  1.0f, 0.0f, 0.0f, 1.0f},
        { 0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f},
        {-0.5f,  0.5f,  1.0f, 1.0f, 1.0f, 1.0f}
    };

    id<MTLBuffer> vertexBuffer = [device newBufferWithBytes:vertices
                                                   length:sizeof(vertices)
                                                  options:MTLStorageModeShared];
    m_VertexBuffer = (intptr_t)CFBridgingRetain(vertexBuffer);

    m_Initialized = true;
    return true;
}

void MetalRenderer::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    if (m_PipelineState != 0)
    {
        CFBridgingRelease((void*)m_PipelineState);
        m_PipelineState = 0;
    }

    if (m_VertexBuffer != 0)
    {
        CFBridgingRelease((void*)m_VertexBuffer);
        m_VertexBuffer = 0;
    }

    if (m_CommandQueue != 0)
    {
        CFBridgingRelease((void*)m_CommandQueue);
        m_CommandQueue = 0;
    }

    if (m_Device != 0)
    {
        CFBridgingRelease((void*)m_Device);
        m_Device = 0;
    }

    if (m_MetalLayer != 0)
    {
        CFBridgingRelease((void*)m_MetalLayer);
        m_MetalLayer = 0;
    }

    m_Initialized = false;
}

void MetalRenderer::BeginFrame()
{
}

void MetalRenderer::EndFrame()
{
    if (!m_Initialized || m_MetalLayer == 0 || m_CommandQueue == 0)
    {
        return;
    }

    CAMetalLayer* metalLayer = (CAMetalLayer*)m_MetalLayer;
    id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
    if (drawable == nil)
    {
        return;
    }

    id<MTLCommandQueue> queue = (id<MTLCommandQueue>)m_CommandQueue;
    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];

    MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    passDesc.colorAttachments[0].texture = drawable.texture;
    passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    passDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.15, 1.0);

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:passDesc];
    [encoder setRenderPipelineState:(id<MTLRenderPipelineState>)m_PipelineState];
    [encoder setVertexBuffer:(id<MTLBuffer>)m_VertexBuffer offset:0 atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    [encoder endEncoding];

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
}

void MetalRenderer::Render(float deltaTime)
{
    m_RotationAngle += deltaTime * 0.5f;
    if (m_RotationAngle > 360.0f)
    {
        m_RotationAngle -= 360.0f;
    }

    float rad = m_RotationAngle * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);

    float r = 0.5f + 0.5f * c;
    float g = 0.5f + 0.5f * s;
    float b = 0.5f - 0.5f * (c + s);

    QuadVertex vertices[6] = {
        {-0.5f, -0.5f,  r, 0.0f, b, 1.0f},
        { 0.5f, -0.5f,  g, r, 0.0f, 1.0f},
        { 0.5f,  0.5f,  0.0f, g, b, 1.0f},
        {-0.5f, -0.5f,  r, 0.0f, b, 1.0f},
        { 0.5f,  0.5f,  0.0f, g, b, 1.0f},
        {-0.5f,  0.5f,  b, r, g, 1.0f}
    };

    if (m_VertexBuffer != 0)
    {
        CFBridgingRelease((void*)m_VertexBuffer);
    }

    id<MTLDevice> device = (id<MTLDevice>)m_Device;
    id<MTLBuffer> newBuffer = [device newBufferWithBytes:vertices
                                               length:sizeof(vertices)
                                              options:MTLStorageModeShared];
    m_VertexBuffer = (intptr_t)CFBridgingRetain(newBuffer);

    (void)deltaTime;
}

APIType MetalRenderer::GetAPIType() const
{
    return m_APIType;
}

RenderCapabilities MetalRenderer::GetCapabilities() const
{
    return m_Capabilities;
}

} // namespace Renderer
} // namespace hbe