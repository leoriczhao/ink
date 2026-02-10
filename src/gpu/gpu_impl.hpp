#pragma once

#include "ink/types.hpp"

namespace ink {

class Image;

/**
 * GpuImpl - Internal abstract base for backend-specific GPU implementations.
 *
 * This is the polymorphic dispatch point hidden behind GpuContext.
 * Each GPU backend (GL, Vulkan, Metal) provides a concrete subclass.
 *
 * NOT part of the public API - lives in src/, not include/.
 */
class GpuImpl {
public:
    virtual ~GpuImpl() = default;

    /**
     * Resolve an Image to a backend-specific GPU texture handle.
     * For GPU-backed images, returns the existing handle.
     * For CPU-backed images, uploads pixel data and caches the result.
     *
     * Returns an opaque u64 handle (e.g. GLuint cast to u64 for GL).
     * Returns 0 on failure.
     */
    virtual u64 resolveImageTexture(const Image* image) = 0;
};

} // namespace ink
