#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Image;
class GLBackend;

/**
 * GpuContext - Lightweight shared GPU resource context.
 *
 * Host owns platform GL context creation/switching (EGL/GLFW/etc).
 * Ink binds to the currently active GL context via MakeGLFromCurrent().
 *
 * This object owns cross-surface shared GPU resources like CPU-image texture
 * cache. Individual GL backends still own per-surface render targets (FBOs).
 */
class GpuContext {
public:
    static std::shared_ptr<GpuContext> MakeGLFromCurrent();

    ~GpuContext();

    bool valid() const { return impl_ != nullptr; }

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;

    explicit GpuContext(std::shared_ptr<Impl> impl);

    // GLBackend internal API
    u32 resolveImageTexture(const Image* image);

    friend class GLBackend;
};

} // namespace ink
