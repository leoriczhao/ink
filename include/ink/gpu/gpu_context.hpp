#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Image;
class GpuImpl;

/**
 * GpuContext - Backend-agnostic shared GPU resource context.
 *
 * Owns cross-surface shared GPU resources (e.g. CPU-image texture cache).
 * Individual backends still own per-surface render targets.
 *
 * Create via backend-specific factory functions:
 *   - GpuContexts::MakeGL() (include ink/gpu/gl/gl_context.hpp)
 *
 * This class is intentionally minimal. All GPU-specific logic is
 * hidden behind GpuImpl (internal).
 */
class GpuContext {
public:
    ~GpuContext();

    bool valid() const;

private:
    std::shared_ptr<GpuImpl> impl_;

    explicit GpuContext(std::shared_ptr<GpuImpl> impl);

    static std::shared_ptr<GpuContext> MakeFromImpl(std::unique_ptr<GpuImpl> impl);
    u64 resolveImageTexture(const Image* image);

    friend class GLBackend;
    friend class GpuImpl;
};

} // namespace ink
