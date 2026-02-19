#pragma once

// Metal resource management utilities.
// Internal implementation - not part of public API.

#if INK_HAS_METAL

#include "ink/types.hpp"
#import <Metal/Metal.h>
#import <simd/simd.h>
#include <unordered_map>
#include <cstdio>

namespace ink {

class Image;

// Orthographic projection matrix (same as GL backend but row-major for Metal)
inline simd_float4x4 metalOrthoProjection(float w, float h) {
    return simd_float4x4{{
        { 2.0f / w,  0,         0, 0 },
        { 0,        -2.0f / h,  0, 0 },
        { 0,         0,        -1, 0 },
        {-1.0f,      1.0f,      0, 1 }
    }};
}

// Color vertex (position + color) - matches GL layout
struct MetalColorVertex {
    float x, y;
    float r, g, b, a;
};

// Texture vertex (position + uv)
struct MetalTexVertex {
    float x, y;
    float u, v;
};

// Pipeline state wrapper for color and texture pipelines
class MetalPipeline {
public:
    id<MTLRenderPipelineState> state = nil;

    bool init(id<MTLDevice> device, id<MTLLibrary> library,
              NSString* vertFunc, NSString* fragFunc,
              MTLPixelFormat pixelFormat,
              MTLVertexDescriptor* vertexDesc) {
        id<MTLFunction> vert = [library newFunctionWithName:vertFunc];
        id<MTLFunction> frag = [library newFunctionWithName:fragFunc];
        if (!vert || !frag) {
            std::fprintf(stderr, "ink Metal: shader function not found\n");
            return false;
        }

        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = vert;
        desc.fragmentFunction = frag;
        desc.vertexDescriptor = vertexDesc;
        desc.colorAttachments[0].pixelFormat = pixelFormat;
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        NSError* error = nil;
        state = [device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (!state) {
            std::fprintf(stderr, "ink Metal: pipeline error: %s\n",
                         [[error localizedDescription] UTF8String]);
            return false;
        }
        return true;
    }

    void destroy() { state = nil; }
};

// Offscreen render target texture
class MetalFramebuffer {
public:
    id<MTLTexture> texture = nil;
    i32 width = 0;
    i32 height = 0;

    bool init(id<MTLDevice> device, i32 w, i32 h) {
        width = w;
        height = h;
        return createTexture(device);
    }

    void destroy() { texture = nil; }

    void resize(id<MTLDevice> device, i32 w, i32 h) {
        width = w;
        height = h;
        createTexture(device);
    }

    MTLRenderPassDescriptor* renderPassDescriptor() const {
        MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = texture;
        desc.colorAttachments[0].loadAction = MTLLoadActionClear;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        desc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        return desc;
    }

private:
    bool createTexture(id<MTLDevice> device) {
        MTLTextureDescriptor* desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:width height:height mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModePrivate;
        texture = [device newTextureWithDescriptor:desc];
        if (!texture) {
            std::fprintf(stderr, "ink Metal: failed to create framebuffer texture\n");
            return false;
        }
        return true;
    }
};

// Texture cache for CPU images
class MetalTextureCache {
public:
    ~MetalTextureCache() { clear(); }

    id<MTLTexture> resolve(id<MTLDevice> device, const Image* image);

    void clear() { cache_.clear(); }

private:
    std::unordered_map<u64, id<MTLTexture>> cache_;
};

} // namespace ink

#endif // INK_HAS_METAL
