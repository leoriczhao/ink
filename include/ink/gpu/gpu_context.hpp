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
 * Create via backend-specific factory methods:
 *   - MakeGL(): binds to the currently active host GL context
 *   - MakeVulkan(): (future) accepts host-created Vulkan objects
 *
 * Internally dispatches to a backend-specific GpuImpl (hidden from public API).
 */
class GpuContext {
public:
    /**
     * Create a GpuContext bound to the currently active OpenGL context.
     * Host must have created and made current a GL context before calling.
     * Returns nullptr if no GL context is current or GL init fails.
     */
    static std::shared_ptr<GpuContext> MakeGL();

    ~GpuContext();

    bool valid() const;

private:
    std::shared_ptr<GpuImpl> impl_;

    explicit GpuContext(std::shared_ptr<GpuImpl> impl);

    /**
     * Resolve an Image to a backend-specific GPU texture handle.
     * Returns an opaque u64 (e.g. GLuint for GL, VkImage handle for Vulkan).
     * Returns 0 on failure.
     */
    u64 resolveImageTexture(const Image* image);

    // All GPU backends need access to resolveImageTexture.
    friend class GLBackend;
};

} // namespace ink
