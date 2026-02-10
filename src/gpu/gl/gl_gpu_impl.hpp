#pragma once

#include <memory>

namespace ink {

class GpuImpl;

/**
 * Create a GL-specific GpuImpl bound to the currently active GL context.
 * Returns nullptr if no GL context is current or GLEW init fails.
 *
 * Internal factory - called by GpuContext::MakeGL().
 */
std::unique_ptr<GpuImpl> makeGLGpuImpl();

} // namespace ink
