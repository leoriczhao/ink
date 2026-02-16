#pragma once

#include "ink/gpu/gpu_context.hpp"

// This header is only usable when INK_HAS_GL is defined.
// Including it without GL support will cause a compile error.

#if !INK_HAS_GL
#error "GL backend not available. Build with -DINK_ENABLE_GL=ON"
#endif

namespace ink {

/**
 * GL-specific GpuContext factory functions.
 *
 * Usage:
 *   #include <ink/gpu/gl/gl_context.hpp>
 *   auto ctx = GpuContexts::MakeGL();
 */
namespace GpuContexts {

/**
 * Create a GpuContext bound to the currently active OpenGL context.
 * Host must have created and made current a GL context before calling.
 * Returns nullptr if no GL context is current or GL init fails.
 */
std::shared_ptr<GpuContext> MakeGL();

} // namespace GpuContexts

} // namespace ink
