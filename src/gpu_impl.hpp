#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Image;
class Recording;
class DrawPass;
class GpuContext;

// Internal base class for GPU implementations.
class GpuImpl {
public:
    virtual ~GpuImpl() = default;

    virtual bool valid() const = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void execute(const Recording& recording, const DrawPass& pass) = 0;
    virtual void resize(i32 w, i32 h) = 0;
    virtual std::shared_ptr<Image> makeSnapshot() const = 0;
    virtual void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const = 0;
    virtual unsigned int textureId() const = 0;
    virtual unsigned int fboId() const = 0;
    virtual u64 resolveImageTexture(const Image* image) = 0;
};

// Internal factory - creates GpuContext from GpuImpl
std::shared_ptr<GpuContext> MakeGpuContextFromImpl(std::shared_ptr<GpuImpl> impl);

} // namespace ink
