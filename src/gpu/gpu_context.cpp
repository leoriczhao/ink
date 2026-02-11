// GpuContext - Backend-agnostic implementation shell.
//
// Delegates to a backend-specific GpuImpl provided by MakeFromImpl().
// This file is always compiled. No backend-specific code or #if guards.

#include "ink/gpu/gpu_context.hpp"
#include "gpu/gpu_impl.hpp"

namespace ink {

GpuContext::GpuContext(std::shared_ptr<GpuImpl> impl)
    : impl_(std::move(impl)) {
}

GpuContext::~GpuContext() = default;

bool GpuContext::valid() const {
    return impl_ != nullptr;
}

std::shared_ptr<GpuContext> GpuContext::MakeFromImpl(std::unique_ptr<GpuImpl> impl) {
    if (!impl) return nullptr;
    return std::shared_ptr<GpuContext>(
        new GpuContext(std::shared_ptr<GpuImpl>(std::move(impl))));
}

u64 GpuContext::resolveImageTexture(const Image* image) {
    if (!impl_) return 0;
    return impl_->resolveImageTexture(image);
}

} // namespace ink
