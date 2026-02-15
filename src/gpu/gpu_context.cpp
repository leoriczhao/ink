// GpuContext - Thin wrapper that forwards to GpuImpl.
//
// This file is always compiled. No backend-specific code.

#include "ink/gpu/gpu_context.hpp"
#include "gpu_impl.hpp"

namespace ink {

GpuContext::GpuContext(std::shared_ptr<GpuImpl> impl) : impl_(std::move(impl)) {}
GpuContext::~GpuContext() = default;

bool GpuContext::valid() const {
    return impl_ && impl_->valid();
}

void GpuContext::beginFrame() { if (impl_) impl_->beginFrame(); }
void GpuContext::endFrame() { if (impl_) impl_->endFrame(); }
void GpuContext::execute(const Recording& r, const DrawPass& p) { if (impl_) impl_->execute(r, p); }
void GpuContext::resize(i32 w, i32 h) { if (impl_) impl_->resize(w, h); }

void GpuContext::setGlyphCache(GlyphCache* cache) {
    if (impl_) impl_->setGlyphCache(cache);
}

std::shared_ptr<Image> GpuContext::makeSnapshot() const {
    return impl_ ? impl_->makeSnapshot() : nullptr;
}

void GpuContext::readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const {
    if (impl_) impl_->readPixels(dst, x, y, w, h);
}

unsigned int GpuContext::textureId() const {
    if (!impl_) return 0;
    auto* interop = impl_->glInterop();
    return interop ? interop->glTextureId() : 0;
}

unsigned int GpuContext::fboId() const {
    if (!impl_) return 0;
    auto* interop = impl_->glInterop();
    return interop ? interop->glFboId() : 0;
}

u64 GpuContext::resolveImageTexture(const Image* image) {
    return impl_ ? impl_->resolveImageTexture(image) : 0;
}

// Factory helper for backend implementations
std::shared_ptr<GpuContext> MakeGpuContextFromImpl(std::shared_ptr<GpuImpl> impl) {
    if (!impl) return nullptr;
    return std::shared_ptr<GpuContext>(new GpuContext(std::move(impl)));
}

} // namespace ink
