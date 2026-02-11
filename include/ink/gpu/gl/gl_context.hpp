#pragma once

#include "ink/gpu/gpu_context.hpp"
#include <memory>

namespace ink {
namespace GpuContexts {

/**
 * Create a GpuContext bound to the currently active OpenGL context.
 * Host must have created and made current a GL context before calling.
 * Returns nullptr if no GL context is current or GL init fails.
 */
std::shared_ptr<GpuContext> MakeGL();

} // namespace GpuContexts
} // namespace ink
