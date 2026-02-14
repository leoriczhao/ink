#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Image;
class Recording;
class DrawPass;

/**
 * GpuContext - GPU rendering context.
 *
 * Manages all GPU resources and executes rendering commands.
 * Create via backend-specific factory functions:
 *   - GpuContexts::MakeGL() (include ink/gpu/gl/gl_context.hpp)
 */
class GpuContext {
public:
    ~GpuContext();

    bool valid() const;

    void beginFrame();
    void endFrame();
    void execute(const Recording& recording, const DrawPass& pass);
    void resize(i32 w, i32 h);

    std::shared_ptr<Image> makeSnapshot() const;
    void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const;

    unsigned int textureId() const;
    unsigned int fboId() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    explicit GpuContext(std::unique_ptr<Impl> impl);

    u64 resolveImageTexture(const Image* image);

    friend class Surface;
    friend std::shared_ptr<GpuContext> MakeGpuContextFromImpl(std::shared_ptr<class GpuImpl>);
};

namespace GpuContexts {
    std::shared_ptr<GpuContext> MakeGL();
}

} // namespace ink
