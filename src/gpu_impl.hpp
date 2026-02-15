#pragma once

#include "ink/renderer.hpp"
#include <memory>

namespace ink {

class Image;
class GpuContext;

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
    virtual unsigned int textureId() const = 0;
    virtual unsigned int fboId() const = 0;
    virtual u64 resolveImageTexture(const Image* image) = 0;
};

// Internal factory - creates GpuContext from GpuImpl
std::shared_ptr<GpuContext> MakeGpuContextFromImpl(std::shared_ptr<GpuImpl> impl);

} // namespace ink
