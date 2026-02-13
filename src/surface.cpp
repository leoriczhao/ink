#include "ink/surface.hpp"
#include "ink/canvas.hpp"
#include "ink/device.hpp"
#include "ink/draw_pass.hpp"
#include "ink/image.hpp"
#include "ink/gpu/gpu_context.hpp"
#include "backend.hpp"
#include "cpu_backend.hpp"

#if INK_HAS_GL
#include "gpu/gl/gl_backend.hpp"
#endif

namespace ink {

Surface::Surface(std::unique_ptr<Backend> backend,
                 std::unique_ptr<Pixmap> pixmap)
    : device_(),
      backend_(std::move(backend)),
      pixmap_(std::move(pixmap)) {
    canvas_ = std::make_unique<Canvas>(&device_);
}

Surface::~Surface() = default;

std::unique_ptr<Surface> Surface::MakeRaster(i32 w, i32 h, PixelFormat fmt) {
    PixmapInfo info = PixmapInfo::Make(w, h, fmt);
    auto pixmap = std::make_unique<Pixmap>(Pixmap::Alloc(info));
    Pixmap* raw = pixmap.get();
    auto backend = std::make_unique<CpuBackend>(raw);
    return std::unique_ptr<Surface>(new Surface(std::move(backend), std::move(pixmap)));
}

std::unique_ptr<Surface> Surface::MakeRasterDirect(const PixmapInfo& info, void* pixels) {
    auto pixmap = std::make_unique<Pixmap>(Pixmap::Wrap(info, pixels));
    Pixmap* raw = pixmap.get();
    auto backend = std::make_unique<CpuBackend>(raw);
    return std::unique_ptr<Surface>(new Surface(std::move(backend), std::move(pixmap)));
}

std::unique_ptr<Surface> Surface::MakeRecording(i32, i32) {
    return std::unique_ptr<Surface>(new Surface(nullptr, nullptr));
}

std::unique_ptr<Surface> Surface::MakeGpu(const std::shared_ptr<GpuContext>& context,
                                          i32 w, i32 h, PixelFormat fmt) {
    if (!context || !context->valid()) {
        return MakeRaster(w, h, fmt);
    }

#if INK_HAS_GL
    auto backend = GLBackend::Make(context, w, h);
    if (backend) {
        return std::unique_ptr<Surface>(new Surface(std::move(backend), nullptr));
    }
#endif

    return MakeRaster(w, h, fmt);
}

void Surface::resize(i32 w, i32 h) {
    if (pixmap_) {
        PixmapInfo info = PixmapInfo::Make(w, h, pixmap_->format());
        pixmap_->reallocate(info);
    }
    if (backend_) {
        backend_->resize(w, h);
    }
}

void Surface::beginFrame() {
    device_.beginFrame();
    if (backend_) {
        backend_->beginFrame();
    }
}

void Surface::endFrame() {
    device_.endFrame();
}

void Surface::flush() {
    auto recording = device_.finishRecording();
    if (!recording) return;

    if (backend_) {
        DrawPass pass = DrawPass::create(*recording);
        backend_->execute(*recording, pass);
    }
}

std::shared_ptr<Image> Surface::makeSnapshot() const {
    if (pixmap_ && pixmap_->valid()) {
        return Image::MakeFromPixmap(*pixmap_);
    }
    if (backend_) {
        return backend_->makeSnapshot();
    }
    return nullptr;
}

bool Surface::isGPU() const {
    return backend_ != nullptr && pixmap_ == nullptr;
}

Pixmap* Surface::peekPixels() {
    return pixmap_.get();
}

const Pixmap* Surface::peekPixels() const {
    return pixmap_.get();
}

PixelData Surface::getPixelData() const {
    if (pixmap_) {
        const auto& info = pixmap_->info();
        return PixelData(pixmap_->addr(), info.width, info.height,
                         info.stride, info.format);
    }
    return PixelData();
}

std::unique_ptr<Recording> Surface::takeRecording() {
    return device_.finishRecording();
}

void Surface::setGlyphCache(GlyphCache* cache) {
    if (backend_) {
        backend_->setGlyphCache(cache);
    }
}

} // namespace ink
