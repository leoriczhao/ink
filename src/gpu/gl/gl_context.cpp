// GL context factory - creates a GpuContext bound to the current GL context.
//
// Only compiled when INK_HAS_GL is defined (via CMake).

#include "ink/gpu/gl/gl_context.hpp"
#include "ink/gpu/gpu_context.hpp"
#include "gpu/gpu_impl.hpp"

namespace ink {

// Defined in gl_gpu_impl.cpp
std::unique_ptr<GpuImpl> makeGLGpuImpl();

namespace GpuContexts {

std::shared_ptr<GpuContext> MakeGL() {
    auto impl = makeGLGpuImpl();
    return GpuImpl::createContext(std::move(impl));
}

} // namespace GpuContexts
} // namespace ink
