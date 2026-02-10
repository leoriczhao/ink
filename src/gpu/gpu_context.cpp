// GpuContext - Backend-agnostic implementation shell.
//
// Delegates to a backend-specific GpuImpl created by the MakeXxx() factories.
// This file is always compiled (no #if guards). Backend availability is
// determined at link time by which gl_gpu_impl / vk_gpu_impl are compiled.

#include "ink/gpu/gpu_context.hpp"
#include "gpu_impl.hpp"

// Forward-declare backend factory functions.
// These are defined in the backend-specific compilation units
// (e.g. src/gpu/gl/gl_gpu_impl.cpp). When a backend is not compiled,
// the stub returns nullptr.
namespace ink {
std::unique_ptr<GpuImpl> makeGLGpuImpl();
}

namespace ink {

GpuContext::GpuContext(std::shared_ptr<GpuImpl> impl)
    : impl_(std::move(impl)) {
}

GpuContext::~GpuContext() = default;

bool GpuContext::valid() const {
    return impl_ != nullptr;
}

std::shared_ptr<GpuContext> GpuContext::MakeGL() {
    auto impl = makeGLGpuImpl();
    if (!impl) return nullptr;
    return std::shared_ptr<GpuContext>(
        new GpuContext(std::shared_ptr<GpuImpl>(std::move(impl))));
}

u64 GpuContext::resolveImageTexture(const Image* image) {
    if (!impl_) return 0;
    return impl_->resolveImageTexture(image);
}

} // namespace ink
