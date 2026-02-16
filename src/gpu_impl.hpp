#pragma once

#include "ink/renderer.hpp"
#include <memory>

namespace ink {

class Image;
class GpuContext;

/**
 * GLInterop - Interface for OpenGL interop operations.
 *
 * Implemented by GLImpl to expose GL-specific handles.
 * Other backends (Vulkan, Metal) would not implement this.
 */
class GLInterop {
public:
    virtual ~GLInterop() = default;
    virtual unsigned int glTextureId() const = 0;
    virtual unsigned int glFboId() const = 0;
};

/**
 * GpuImpl - Internal base class for GPU implementations.
 *
 * Extends Renderer with GPU-specific operations.
 * Implemented by GLImpl, VulkanImpl, etc.
 */
class GpuImpl : public Renderer {
public:
    ~GpuImpl() override = default;

    virtual bool valid() const = 0;
    virtual void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const = 0;
    virtual u64 resolveImageTexture(const Image* image) = 0;
    virtual GLInterop* glInterop() { return nullptr; }

    // Default no-op; overridden by backends that support text rendering
    void setGlyphCache(GlyphCache*) override {}
};

// Internal factory - creates GpuContext from GpuImpl
std::shared_ptr<GpuContext> MakeGpuContextFromImpl(std::shared_ptr<GpuImpl> impl);

} // namespace ink
