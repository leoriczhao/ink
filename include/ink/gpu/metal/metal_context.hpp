#pragma once

#include "ink/gpu/gpu_context.hpp"

// This header is only usable when INK_HAS_METAL is defined.
// Including it without Metal support will cause a compile error.

#if !INK_HAS_METAL
#error "Metal backend not available. Build with -DINK_ENABLE_METAL=ON on Apple platforms"
#endif

namespace ink {

/**
 * Metal-specific GpuContext factory functions.
 *
 * Usage:
 *   #include <ink/gpu/metal/metal_context.hpp>
 *   auto ctx = GpuContexts::MakeMetal();
 */
namespace GpuContexts {

/**
 * Create a GpuContext using the default Metal device.
 * Returns nullptr if no Metal-capable GPU is available or init fails.
 */
std::shared_ptr<GpuContext> MakeMetal();

} // namespace GpuContexts

} // namespace ink
