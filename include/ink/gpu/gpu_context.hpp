#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Image;
class GpuImpl;

// Forward declare for friend access.
class GpuContext;
namespace GpuContexts {
std::shared_ptr<GpuContext> MakeGL();
}

/**
 * GpuContext - Backend-agnostic shared GPU resource context.
 *
 * Owns cross-surface shared GPU resources (e.g. CPU-image texture cache).
 * Individual backends still own per-surface render targets.
 *
 * Create via backend-specific factory functions in the GpuContexts namespace:
 *   - GpuContexts::MakeGL()      (include ink/gpu/gl/gl_context.hpp)
 *   - GpuContexts::MakeVulkan()  (future)
 *
 * Internally dispatches to a backend-specific GpuImpl (hidden from public API).
 */
class GpuContext {
public:
    ~GpuContext();

    bool valid() const;

private:
    std::shared_ptr<GpuImpl> impl_;

    explicit GpuContext(std::shared_ptr<GpuImpl> impl);

    /**
     * Internal factory used by backend-specific MakeXxx() functions.
     * Returns nullptr if impl is null.
     */
    static std::shared_ptr<GpuContext> MakeFromImpl(std::unique_ptr<GpuImpl> impl);

    /**
     * Resolve an Image to a backend-specific GPU texture handle.
     * Returns an opaque u64 (e.g. GLuint for GL, VkImage handle for Vulkan).
     * Returns 0 on failure.
     */
    u64 resolveImageTexture(const Image* image);

    friend class GLBackend;
    friend std::shared_ptr<GpuContext> GpuContexts::MakeGL();
};

} // namespace ink
