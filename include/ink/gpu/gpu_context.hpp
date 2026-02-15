#pragma once

#include "ink/renderer.hpp"
#include <memory>

namespace ink {

class GpuImpl;

/**
 * GpuContext - GPU rendering context.
 *
 * Implements Renderer interface for GPU rendering.
 * Create via backend-specific factory functions:
 *   - GpuContexts::MakeGL() (include ink/gpu/gl/gl_context.hpp)
 */
class GpuContext : public Renderer {
public:
    ~GpuContext() override;

    bool valid() const;

    // Renderer interface
    void beginFrame() override;
    void endFrame() override;
    void execute(const Recording& recording, const DrawPass& pass) override;
    void resize(i32 w, i32 h) override;
    std::shared_ptr<Image> makeSnapshot() const override;

    // GPU-specific operations
    void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const;
    unsigned int textureId() const;
    unsigned int fboId() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    explicit GpuContext(std::unique_ptr<Impl> impl);

    u64 resolveImageTexture(const Image* image);

    friend class Surface;
    friend std::shared_ptr<GpuContext> MakeGpuContextFromImpl(std::shared_ptr<GpuImpl>);
};

namespace GpuContexts {
    std::shared_ptr<GpuContext> MakeGL();
}

} // namespace ink
