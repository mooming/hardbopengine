// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "MetalRenderer.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#include <cmath>
#include <iostream>


namespace hbe {
namespace Renderer {

struct QuadVertex {
    float x, y;
    float r, g, b, a;
};

MetalRenderer::MetalRenderer() noexcept
    : apiType(APIType::Metal)
{
    capabilities.apiType = APIType::Metal;
    capabilities.supportsComputeShader = true;
    capabilities.maxTextureSize = 4096;
    capabilities.maxVertexAttribs = 31;
}

MetalRenderer::~MetalRenderer()
{
    Shutdown();
}

bool MetalRenderer::Initialize(OS::IWindow* window) noexcept
{
    if (initialized) return true;

    if (window == nullptr)
    {
        std::cerr << "Error: MetalRenderer::Initialize - window is null" << std::endl;
        return false;
    }

    this->window = window;

    NSWindow* nsWindow = (NSWindow*)window->GetNativeHandle();
    if (nsWindow == nil)
    {
        std::cerr << "Error: MetalRenderer::Initialize - failed to get native window handle" << std::endl;
        return false;
    }

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (device == nil)
    {
        std::cerr << "Error: MetalRenderer::Initialize - Metal is not supported on this device" << std::endl;
        return false;
    }
    this->device = (intptr_t)CFBridgingRetain(device);

    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (queue == nil)
    {
        std::cerr << "Error: MetalRenderer::Initialize - failed to create command queue" << std::endl;
        return false;
    }
    commandQueue = (intptr_t)CFBridgingRetain(queue);

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
    this->metalLayer = (intptr_t)CFBridgingRetain(metalLayer);

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
        std::cerr << "Error: MetalRenderer::Initialize - failed to compile shaders" << std::endl;
        return false;
    }

    id<MTLFunction> vertexFunc = [library newFunctionWithName:@"vertexShader"];
    id<MTLFunction> fragmentFunc = [library newFunctionWithName:@"fragmentShader"];

    if (vertexFunc == nil || fragmentFunc == nil)
    {
        std::cerr << "Error: MetalRenderer::Initialize - failed to find shader functions" << std::endl;
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
        std::cerr << "Error: MetalRenderer::Initialize - failed to create pipeline state" << std::endl;
        return false;
    }

    pipelineState = (intptr_t)CFBridgingRetain(pipeline);

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
    this->vertexBuffer = (intptr_t)CFBridgingRetain(vertexBuffer);

    initialized = true;

    return true;
}

void MetalRenderer::Shutdown() noexcept
{
    if (!initialized) return;

    if (pipelineState != 0)
    {
        CFBridgingRelease((void*)pipelineState);
        pipelineState = 0;
    }

    if (vertexBuffer != 0)
    {
        CFBridgingRelease((void*)vertexBuffer);
        vertexBuffer = 0;
    }

    if (commandQueue != 0)
    {
        CFBridgingRelease((void*)commandQueue);
        commandQueue = 0;
    }

    if (device != 0)
    {
        CFBridgingRelease((void*)device);
        device = 0;
    }

    if (metalLayer != 0)
    {
        CFBridgingRelease((void*)metalLayer);
        metalLayer = 0;
    }

    initialized = false;
}

void MetalRenderer::BeginFrame() noexcept
{
}

void MetalRenderer::EndFrame() noexcept
{
    if (!initialized || metalLayer == 0 || commandQueue == 0) return;

    CAMetalLayer* metalLayer = (CAMetalLayer*)this->metalLayer;
    id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
    if (drawable == nil) return;

    id<MTLCommandQueue> queue = (id<MTLCommandQueue>)commandQueue;
    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];

    MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    passDesc.colorAttachments[0].texture = drawable.texture;
    passDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    passDesc.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.15, 1.0);

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:passDesc];
    [encoder setRenderPipelineState:(id<MTLRenderPipelineState>)pipelineState];
    [encoder setVertexBuffer:(id<MTLBuffer>)vertexBuffer offset:0 atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    [encoder endEncoding];

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
}

void MetalRenderer::Render(float deltaTime) noexcept
{
    rotationAngle += deltaTime * 90.0f;
    if (rotationAngle > 360.0f)
    {
        rotationAngle -= 360.0f;
    }

    float rad = rotationAngle * M_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    float x1 = -0.5f * c - (-0.5f) * s;
    float y1 = -0.5f * s + (-0.5f) * c;
    float x2 = 0.5f * c - (-0.5f) * s;
    float y2 = 0.5f * s + (-0.5f) * c;
    float x3 = 0.5f * c - 0.5f * s;
    float y3 = 0.5f * s + 0.5f * c;
    float x4 = -0.5f * c - 0.5f * s;
    float y4 = -0.5f * s + 0.5f * c;

    QuadVertex vertices[6] = {
        {x1, y1,  1.0f, 0.0f, 0.0f, 1.0f},
        {x2, y2,  0.0f, 1.0f, 0.0f, 1.0f},
        {x3, y3,  0.0f, 0.0f, 1.0f, 1.0f},
        {x1, y1,  1.0f, 0.0f, 0.0f, 1.0f},
        {x3, y3,  0.0f, 0.0f, 1.0f, 1.0f},
        {x4, y4,  1.0f, 1.0f, 1.0f, 1.0f}
    };

    if (vertexBuffer != 0)
    {
        CFBridgingRelease((void*)vertexBuffer);
    }

    id<MTLDevice> device = (id<MTLDevice>)this->device;
    id<MTLBuffer> newBuffer = [device newBufferWithBytes:vertices
                                               length:sizeof(vertices)
                                              options:MTLStorageModeShared];
    vertexBuffer = (intptr_t)CFBridgingRetain(newBuffer);

    (void)deltaTime;
}

APIType MetalRenderer::GetAPIType() const noexcept
{
    return apiType;
}

RenderCapabilities MetalRenderer::GetCapabilities() const noexcept
{
    return capabilities;
}

} // namespace Renderer
} // namespace hbe
