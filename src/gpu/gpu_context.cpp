// GpuContext - Backend-agnostic implementation shell.
//
// Delegates to a backend-specific GpuImpl created by the MakeXxx() factories.
// This file is always compiled. Backend availability is determined at compile
// time via INK_HAS_GL / INK_HAS_VULKAN defines.

#include "ink/gpu/gpu_context.hpp"
#include "gpu_impl.hpp"

#if INK_HAS_GL
// Defined in src/gpu/gl/gl_gpu_impl.cpp (only compiled when GL is enabled).
namespace ink { std::unique_ptr<GpuImpl> makeGLGpuImpl(); }
#endif

namespace ink {

GpuContext::GpuContext(std::shared_ptr<GpuImpl> impl)
    : impl_(std::move(impl)) {
}

GpuContext::~GpuContext() = default;

bool GpuContext::valid() const {
    return impl_ != nullptr;
}

std::shared_ptr<GpuContext> GpuContext::MakeGL() {
#if INK_HAS_GL
    auto impl = makeGLGpuImpl();
    if (!impl) return nullptr;
    return std::shared_ptr<GpuContext>(
        new GpuContext(std::shared_ptr<GpuImpl>(std::move(impl))));
#else
    return nullptr;
#endif
}

u64 GpuContext::resolveImageTexture(const Image* image) {
    if (!impl_) return 0;
    return impl_->resolveImageTexture(image);
}

} // namespace ink
