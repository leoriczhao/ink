// Metal context implementation - implements GpuImpl for Metal.
//
// Only compiled when INK_HAS_METAL is defined (via CMake).
// Requires macOS 10.13+ or iOS 11+.

#include "ink/gpu/metal/metal_context.hpp"
#include "ink/gpu/gpu_context.hpp"
#include "ink/recording.hpp"
#include "ink/draw_pass.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/image.hpp"
#include "ink/glyph_cache.hpp"
#include "gpu_impl.hpp"
#include "metal_resources.hpp"

#if INK_HAS_METAL

#import <Metal/Metal.h>
#import <simd/simd.h>
#include <vector>
#include <cstdio>
#include <cmath>

namespace ink {

// Metal Shading Language source (embedded)
static NSString* const kShaderSource = @R"(
#include <metal_stdlib>
using namespace metal;

struct ColorVertex {
    float2 position [[attribute(0)]];
    float4 color    [[attribute(1)]];
};

struct ColorOut {
    float4 position [[position]];
    float4 color;
};

struct TexVertex {
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

struct TexOut {
    float4 position [[position]];
    float2 texCoord;
};

struct Uniforms {
    float4x4 projection;
};

vertex ColorOut color_vertex(ColorVertex in [[stage_in]],
                             constant Uniforms& uniforms [[buffer(1)]]) {
    ColorOut out;
    out.position = uniforms.projection * float4(in.position, 0.0, 1.0);
    out.color = in.color;
    return out;
}

fragment float4 color_fragment(ColorOut in [[stage_in]]) {
    return in.color;
}

vertex TexOut tex_vertex(TexVertex in [[stage_in]],
                         constant Uniforms& uniforms [[buffer(1)]]) {
    TexOut out;
    out.position = uniforms.projection * float4(in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 tex_fragment(TexOut in [[stage_in]],
                             texture2d<float> tex [[texture(0)]],
                             sampler samp [[sampler(0)]]) {
    return tex.sample(samp, in.texCoord);
}
)";

// MetalTextureCache::resolve implementation
id<MTLTexture> MetalTextureCache::resolve(id<MTLDevice> device, const Image* image) {
    if (!image || !image->valid()) return nil;
    if (image->isGpuBacked()) {
        // The backend texture handle stores a bridged MTLTexture pointer
        u64 handle = image->backendTextureHandle();
        if (handle == 0) return nil;
        return (__bridge id<MTLTexture>)(void*)handle;
    }

    u64 imgId = image->uniqueId();
    auto it = cache_.find(imgId);
    if (it != cache_.end()) return it->second;

    MTLPixelFormat fmt = (image->format() == PixelFormat::BGRA8888)
        ? MTLPixelFormatBGRA8Unorm : MTLPixelFormatRGBA8Unorm;
    MTLTextureDescriptor* desc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:fmt
        width:image->width() height:image->height() mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;

    id<MTLTexture> mtlTex = [device newTextureWithDescriptor:desc];
    if (!mtlTex) return nil;

    MTLRegion region = MTLRegionMake2D(0, 0, image->width(), image->height());
    [mtlTex replaceRegion:region mipmapLevel:0
              withBytes:image->pixels()
            bytesPerRow:image->stride()];

    cache_[imgId] = mtlTex;
    return mtlTex;
}

// MetalImpl - implements GpuImpl + DrawOpVisitor for Metal
class MetalImpl : public GpuImpl, public DrawOpVisitor {
public:
    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> commandQueue_ = nil;
    id<MTLLibrary> library_ = nil;
    id<MTLSamplerState> sampler_ = nil;

    MetalPipeline colorPipeline_;
    MetalPipeline texPipeline_;
    MetalFramebuffer framebuffer_;
    MetalTextureCache textureCache_;
    GlyphCache* glyphCache_ = nullptr;

    // Per-frame state
    id<MTLCommandBuffer> commandBuffer_ = nil;
    id<MTLRenderCommandEncoder> encoder_ = nil;

    std::vector<MetalColorVertex> colorVerts_;
    std::vector<MetalTexVertex> texVerts_;
    const Recording* currentRecording_ = nullptr;

    // Temp texture for text rendering
    id<MTLTexture> tempTexture_ = nil;

    ~MetalImpl() override { destroy(); }

    bool init(i32 w, i32 h) {
        device_ = MTLCreateSystemDefaultDevice();
        if (!device_) {
            std::fprintf(stderr, "ink Metal: no Metal device available\n");
            return false;
        }

        commandQueue_ = [device_ newCommandQueue];
        if (!commandQueue_) {
            std::fprintf(stderr, "ink Metal: failed to create command queue\n");
            return false;
        }

        // Compile shaders from source
        NSError* error = nil;
        library_ = [device_ newLibraryWithSource:kShaderSource options:nil error:&error];
        if (!library_) {
            std::fprintf(stderr, "ink Metal: shader compile error: %s\n",
                         [[error localizedDescription] UTF8String]);
            return false;
        }

        // Create sampler
        MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
        samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
        samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
        sampler_ = [device_ newSamplerStateWithDescriptor:samplerDesc];

        // Color vertex descriptor: float2 position + float4 color, stride = 24
        MTLVertexDescriptor* colorVertDesc = [[MTLVertexDescriptor alloc] init];
        colorVertDesc.attributes[0].format = MTLVertexFormatFloat2;
        colorVertDesc.attributes[0].offset = 0;
        colorVertDesc.attributes[0].bufferIndex = 0;
        colorVertDesc.attributes[1].format = MTLVertexFormatFloat4;
        colorVertDesc.attributes[1].offset = sizeof(float) * 2;
        colorVertDesc.attributes[1].bufferIndex = 0;
        colorVertDesc.layouts[0].stride = sizeof(MetalColorVertex);
        colorVertDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        // Texture vertex descriptor: float2 position + float2 texCoord, stride = 16
        MTLVertexDescriptor* texVertDesc = [[MTLVertexDescriptor alloc] init];
        texVertDesc.attributes[0].format = MTLVertexFormatFloat2;
        texVertDesc.attributes[0].offset = 0;
        texVertDesc.attributes[0].bufferIndex = 0;
        texVertDesc.attributes[1].format = MTLVertexFormatFloat2;
        texVertDesc.attributes[1].offset = sizeof(float) * 2;
        texVertDesc.attributes[1].bufferIndex = 0;
        texVertDesc.layouts[0].stride = sizeof(MetalTexVertex);
        texVertDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        // Create pipelines
        if (!colorPipeline_.init(device_, library_, @"color_vertex", @"color_fragment",
                                 MTLPixelFormatBGRA8Unorm, colorVertDesc))
            return false;
        if (!texPipeline_.init(device_, library_, @"tex_vertex", @"tex_fragment",
                               MTLPixelFormatBGRA8Unorm, texVertDesc))
            return false;

        return framebuffer_.init(device_, w, h);
    }

    void destroy() {
        textureCache_.clear();
        framebuffer_.destroy();
        colorPipeline_.destroy();
        texPipeline_.destroy();
        tempTexture_ = nil;
        sampler_ = nil;
        library_ = nil;
        commandQueue_ = nil;
        device_ = nil;
    }

    // GpuImpl interface
    bool valid() const override { return framebuffer_.texture != nil; }

    void beginFrame() override {
        // Commit any pending work from a previous frame that was never flushed
        commitIfNeeded();

        commandBuffer_ = [commandQueue_ commandBuffer];
        MTLRenderPassDescriptor* rpDesc = framebuffer_.renderPassDescriptor();
        encoder_ = [commandBuffer_ renderCommandEncoderWithDescriptor:rpDesc];
        MTLViewport vp = {0, 0,
            (double)framebuffer_.width, (double)framebuffer_.height, 0, 1};
        [encoder_ setViewport:vp];
    }

    void endFrame() override {
        flushColorBatch();
        // Don't commit here — Surface::flush() calls execute() after endFrame(),
        // and execute() needs the active encoder. Commit happens in execute().
    }

    void resize(i32 w, i32 h) override {
        framebuffer_.resize(device_, w, h);
    }

    void setGlyphCache(GlyphCache* cache) override { glyphCache_ = cache; }

    void execute(const Recording& recording, const DrawPass& pass) override {
        currentRecording_ = &recording;
        recording.dispatch(*this, pass);
        flushColorBatch();
        currentRecording_ = nullptr;

        // Commit the command buffer after executing all draw ops
        commitIfNeeded();
    }

    // DrawOpVisitor interface
    void visitFillRect(Rect r, Color c) override {
        pushQuad(r.x, r.y, r.x + r.w, r.y + r.h, c);
    }

    void visitStrokeRect(Rect r, Color c, f32 width) override {
        float w = width > 0 ? width : 1.0f;
        pushQuad(r.x, r.y, r.x + r.w, r.y + w, c);
        pushQuad(r.x, r.y + r.h - w, r.x + r.w, r.y + r.h, c);
        pushQuad(r.x, r.y + w, r.x + w, r.y + r.h - w, c);
        pushQuad(r.x + r.w - w, r.y + w, r.x + r.w, r.y + r.h - w, c);
    }

    void visitLine(Point p1, Point p2, Color c, f32 width) override {
        pushLine(p1.x, p1.y, p2.x, p2.y, c, width > 0 ? width : 1.0f);
    }

    void visitPolyline(const Point* pts, i32 count, Color c, f32 width) override {
        float w = width > 0 ? width : 1.0f;
        for (i32 i = 0; i + 1 < count; ++i)
            pushLine(pts[i].x, pts[i].y, pts[i+1].x, pts[i+1].y, c, w);
    }

    void visitText(Point p, const char* text, u32 len, Color c) override {
        flushColorBatch();
        if (glyphCache_) {
            i32 tw = glyphCache_->measureText(std::string_view(text, len));
            i32 th = glyphCache_->lineHeight();
            if (tw > 0 && th > 0) {
                std::vector<u32> buf(tw * th, 0);
                glyphCache_->drawText(buf.data(), tw, th, 0, 0, std::string_view(text, len), c);
                ensureTempTexture(tw, th);
                MTLRegion region = MTLRegionMake2D(0, 0, tw, th);
                [tempTexture_ replaceRegion:region mipmapLevel:0
                                  withBytes:buf.data() bytesPerRow:tw * 4];
                float x = p.x, y = p.y - th;
                pushTexQuad(x, y, x + tw, y + th, 0, 0, 1, 1);
                flushTexBatch(tempTexture_);
            }
        }
    }

    void visitDrawImage(const Image* image, f32 x, f32 y) override {
        flushColorBatch();
        if (image && image->valid()) {
            id<MTLTexture> tex = textureCache_.resolve(device_, image);
            if (tex) {
                float w = float(image->width()), h = float(image->height());
                pushTexQuad(x, y, x + w, y + h, 0, 0, 1, 1);
                flushTexBatch(tex);
            }
        }
    }

    void visitSetClip(Rect r) override {
        flushColorBatch();
        if (encoder_) {
            NSUInteger fbW = NSUInteger(framebuffer_.width);
            NSUInteger fbH = NSUInteger(framebuffer_.height);

            NSUInteger sx = NSUInteger(std::max(0.0f, r.x));
            NSUInteger sy = NSUInteger(std::max(0.0f, r.y));

            // Clamp origin to framebuffer bounds to avoid unsigned underflow
            if (sx >= fbW) sx = fbW;
            if (sy >= fbH) sy = fbH;

            NSUInteger sw = NSUInteger(std::max(0.0f, r.w));
            NSUInteger sh = NSUInteger(std::max(0.0f, r.h));

            if (sx + sw > fbW) sw = fbW - sx;
            if (sy + sh > fbH) sh = fbH - sy;

            MTLScissorRect scissor = { sx, sy, sw, sh };
            [encoder_ setScissorRect:scissor];
        }
    }

    void visitClearClip() override {
        flushColorBatch();
        if (encoder_) {
            MTLScissorRect fullRect;
            fullRect.x = 0;
            fullRect.y = 0;
            fullRect.width = NSUInteger(framebuffer_.width);
            fullRect.height = NSUInteger(framebuffer_.height);
            [encoder_ setScissorRect:fullRect];
        }
    }

    std::shared_ptr<Image> makeSnapshot() const override {
        if (framebuffer_.width <= 0 || framebuffer_.height <= 0 || !framebuffer_.texture)
            return nullptr;

        // Create a managed texture to read back
        MTLTextureDescriptor* desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:framebuffer_.width height:framebuffer_.height mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModeManaged;

        id<MTLTexture> snapshot = [device_ newTextureWithDescriptor:desc];
        if (!snapshot) return nullptr;

        // Blit copy
        id<MTLCommandBuffer> cmdBuf = [commandQueue_ commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit copyFromTexture:framebuffer_.texture
                  sourceSlice:0 sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(framebuffer_.width, framebuffer_.height, 1)
                    toTexture:snapshot
             destinationSlice:0 destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit synchronizeTexture:snapshot slice:0 level:0];
        [blit endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];

        // Read pixels into CPU buffer
        i32 w = framebuffer_.width, h = framebuffer_.height;
        auto pixmap = Pixmap::Alloc(PixmapInfo::MakeBGRA(w, h));
        MTLRegion region = MTLRegionMake2D(0, 0, w, h);
        [snapshot getBytes:pixmap.addr() bytesPerRow:w * 4 fromRegion:region mipmapLevel:0];

        return Image::MakeFromPixmap(pixmap);
    }

    void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const override {
        if (!framebuffer_.texture || !dst) return;

        // Create managed texture for readback
        MTLTextureDescriptor* desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:w height:h mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModeManaged;

        id<MTLTexture> readTex = [device_ newTextureWithDescriptor:desc];
        if (!readTex) return;

        id<MTLCommandBuffer> cmdBuf = [commandQueue_ commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit copyFromTexture:framebuffer_.texture
                  sourceSlice:0 sourceLevel:0
                 sourceOrigin:MTLOriginMake(x, y, 0)
                   sourceSize:MTLSizeMake(w, h, 1)
                    toTexture:readTex
             destinationSlice:0 destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit synchronizeTexture:readTex slice:0 level:0];
        [blit endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];

        MTLRegion region = MTLRegionMake2D(0, 0, w, h);
        [readTex getBytes:dst bytesPerRow:w * 4 fromRegion:region mipmapLevel:0];
    }

    u64 resolveImageTexture(const Image* image) override {
        id<MTLTexture> tex = textureCache_.resolve(device_, image);
        // Return as opaque handle
        return tex ? reinterpret_cast<u64>((__bridge void*)tex) : 0;
    }

    // Batching helpers
    void pushQuad(float x0, float y0, float x1, float y1, Color c) {
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        colorVerts_.insert(colorVerts_.end(), {
            {x0,y0,r,g,b,a}, {x1,y0,r,g,b,a}, {x0,y1,r,g,b,a},
            {x1,y0,r,g,b,a}, {x1,y1,r,g,b,a}, {x0,y1,r,g,b,a}
        });
    }

    void pushLine(float x0, float y0, float x1, float y1, Color c, float width) {
        float dx = x1-x0, dy = y1-y0, len = std::sqrt(dx*dx + dy*dy);
        if (len < 0.0001f) return;
        float hw = width * 0.5f, nx = -dy/len*hw, ny = dx/len*hw;
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        colorVerts_.insert(colorVerts_.end(), {
            {x0+nx,y0+ny,r,g,b,a}, {x0-nx,y0-ny,r,g,b,a}, {x1+nx,y1+ny,r,g,b,a},
            {x0-nx,y0-ny,r,g,b,a}, {x1-nx,y1-ny,r,g,b,a}, {x1+nx,y1+ny,r,g,b,a}
        });
    }

    void pushTexQuad(float x0, float y0, float x1, float y1,
                     float u0, float v0, float u1, float v1) {
        texVerts_.insert(texVerts_.end(), {
            {x0,y0,u0,v0}, {x1,y0,u1,v0}, {x0,y1,u0,v1},
            {x1,y0,u1,v0}, {x1,y1,u1,v1}, {x0,y1,u0,v1}
        });
    }

    void flushColorBatch() {
        if (colorVerts_.empty() || !encoder_) return;

        simd_float4x4 proj = metalOrthoProjection(
            float(framebuffer_.width), float(framebuffer_.height));

        [encoder_ setRenderPipelineState:colorPipeline_.state];
        NSUInteger dataLen = colorVerts_.size() * sizeof(MetalColorVertex);
        if (dataLen <= 4096) {
            [encoder_ setVertexBytes:colorVerts_.data() length:dataLen atIndex:0];
        } else {
            id<MTLBuffer> buf = [device_ newBufferWithBytes:colorVerts_.data()
                                                    length:dataLen
                                                   options:MTLResourceStorageModeShared];
            [encoder_ setVertexBuffer:buf offset:0 atIndex:0];
        }
        [encoder_ setVertexBytes:&proj length:sizeof(proj) atIndex:1];
        [encoder_ drawPrimitives:MTLPrimitiveTypeTriangle
                     vertexStart:0
                     vertexCount:colorVerts_.size()];
        colorVerts_.clear();
    }

    void commitIfNeeded() {
        if (encoder_) {
            [encoder_ endEncoding];
            encoder_ = nil;
        }
        if (commandBuffer_) {
            [commandBuffer_ commit];
            [commandBuffer_ waitUntilCompleted];
            commandBuffer_ = nil;
        }
    }

    void flushTexBatch(id<MTLTexture> tex) {
        if (texVerts_.empty() || !encoder_ || !tex) return;

        simd_float4x4 proj = metalOrthoProjection(
            float(framebuffer_.width), float(framebuffer_.height));

        [encoder_ setRenderPipelineState:texPipeline_.state];
        NSUInteger dataLen = texVerts_.size() * sizeof(MetalTexVertex);
        if (dataLen <= 4096) {
            [encoder_ setVertexBytes:texVerts_.data() length:dataLen atIndex:0];
        } else {
            id<MTLBuffer> buf = [device_ newBufferWithBytes:texVerts_.data()
                                                    length:dataLen
                                                   options:MTLResourceStorageModeShared];
            [encoder_ setVertexBuffer:buf offset:0 atIndex:0];
        }
        [encoder_ setVertexBytes:&proj length:sizeof(proj) atIndex:1];
        [encoder_ setFragmentTexture:tex atIndex:0];
        [encoder_ setFragmentSamplerState:sampler_ atIndex:0];
        [encoder_ drawPrimitives:MTLPrimitiveTypeTriangle
                     vertexStart:0
                     vertexCount:texVerts_.size()];
        texVerts_.clear();
    }

    void ensureTempTexture(i32 w, i32 h) {
        if (tempTexture_ &&
            (i32)[tempTexture_ width] >= w &&
            (i32)[tempTexture_ height] >= h) {
            return;
        }
        MTLTextureDescriptor* desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:w height:h mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead;
        tempTexture_ = [device_ newTextureWithDescriptor:desc];
    }
};

// Factory
namespace GpuContexts {

std::shared_ptr<GpuContext> MakeMetal() {
    auto impl = std::make_shared<MetalImpl>();
    if (!impl->init(1, 1)) return nullptr;
    return MakeGpuContextFromImpl(impl);
}

} // namespace GpuContexts

} // namespace ink

#endif // INK_HAS_METAL
