// GpuContext - Thin wrapper that forwards to GpuImpl.
//
// This file is always compiled. No backend-specific code.

#include "ink/gpu/gpu_context.hpp"
#include "gpu_impl.hpp"

namespace ink {

// GpuContext::Impl just wraps GpuImpl for type erasure
class GpuContext::Impl {
public:
    std::shared_ptr<GpuImpl> gpu;
    explicit Impl(std::shared_ptr<GpuImpl> g) : gpu(std::move(g)) {}
};

GpuContext::GpuContext(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
GpuContext::~GpuContext() = default;

bool GpuContext::valid() const {
    return impl_ && impl_->gpu && impl_->gpu->valid();
}

void GpuContext::beginFrame() { if (impl_->gpu) impl_->gpu->beginFrame(); }
void GpuContext::endFrame() { if (impl_->gpu) impl_->gpu->endFrame(); }
void GpuContext::execute(const Recording& r, const DrawPass& p) { if (impl_->gpu) impl_->gpu->execute(r, p); }
void GpuContext::resize(i32 w, i32 h) { if (impl_->gpu) impl_->gpu->resize(w, h); }

std::shared_ptr<Image> GpuContext::makeSnapshot() const {
    return impl_ && impl_->gpu ? impl_->gpu->makeSnapshot() : nullptr;
}

void GpuContext::readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const {
    if (impl_ && impl_->gpu) impl_->gpu->readPixels(dst, x, y, w, h);
}

unsigned int GpuContext::textureId() const {
    if (!impl_ || !impl_->gpu) return 0;
    auto* interop = impl_->gpu->glInterop();
    return interop ? interop->glTextureId() : 0;
}

unsigned int GpuContext::fboId() const {
    if (!impl_ || !impl_->gpu) return 0;
    auto* interop = impl_->gpu->glInterop();
    return interop ? interop->glFboId() : 0;
}

u64 GpuContext::resolveImageTexture(const Image* image) {
    return impl_ && impl_->gpu ? impl_->gpu->resolveImageTexture(image) : 0;
}

// Factory helper for backend implementations
std::shared_ptr<GpuContext> MakeGpuContextFromImpl(std::shared_ptr<GpuImpl> impl) {
    if (!impl) return nullptr;
    auto contextImpl = std::make_unique<GpuContext::Impl>(std::move(impl));
    return std::shared_ptr<GpuContext>(new GpuContext(std::move(contextImpl)));
}

} // namespace ink
