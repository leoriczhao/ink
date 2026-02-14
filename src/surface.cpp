#include "ink/surface.hpp"
#include "ink/canvas.hpp"
#include "ink/device.hpp"
#include "ink/draw_pass.hpp"
#include "ink/image.hpp"
#include "ink/gpu/gpu_context.hpp"
#include "cpu_renderer.hpp"

namespace ink {

Surface::Surface(std::shared_ptr<GpuContext> gpuContext, std::unique_ptr<Pixmap> pixmap)
    : device_(),
      gpuContext_(std::move(gpuContext)),
      pixmap_(std::move(pixmap)) {
    canvas_ = std::make_unique<Canvas>(&device_);
}

Surface::~Surface() = default;

std::unique_ptr<Surface> Surface::MakeRaster(i32 w, i32 h, PixelFormat fmt) {
    PixmapInfo info = PixmapInfo::Make(w, h, fmt);
    auto pixmap = std::make_unique<Pixmap>(Pixmap::Alloc(info));
    return std::unique_ptr<Surface>(new Surface(nullptr, std::move(pixmap)));
}

std::unique_ptr<Surface> Surface::MakeRasterDirect(const PixmapInfo& info, void* pixels) {
    auto pixmap = std::make_unique<Pixmap>(Pixmap::Wrap(info, pixels));
    return std::unique_ptr<Surface>(new Surface(nullptr, std::move(pixmap)));
}

std::unique_ptr<Surface> Surface::MakeRecording(i32, i32) {
    return std::unique_ptr<Surface>(new Surface(nullptr, nullptr));
}

std::unique_ptr<Surface> Surface::MakeGpu(const std::shared_ptr<GpuContext>& context,
                                          i32 w, i32 h, PixelFormat fmt) {
    if (!context || !context->valid()) {
        return MakeRaster(w, h, fmt);
    }

    context->resize(w, h);
    return std::unique_ptr<Surface>(new Surface(context, nullptr));
}

void Surface::resize(i32 w, i32 h) {
    if (pixmap_) {
        PixmapInfo info = PixmapInfo::Make(w, h, pixmap_->format());
        pixmap_->reallocate(info);
    }
    if (gpuContext_) {
        gpuContext_->resize(w, h);
    }
}

void Surface::beginFrame() {
    device_.beginFrame();
    if (gpuContext_) {
        gpuContext_->beginFrame();
    } else if (pixmap_) {
        pixmap_->clear({0, 0, 0, 255});
    }
}

void Surface::endFrame() {
    device_.endFrame();
    if (gpuContext_) {
        gpuContext_->endFrame();
    }
}

void Surface::flush() {
    auto recording = device_.finishRecording();
    if (!recording) return;

    DrawPass pass = DrawPass::create(*recording);

    if (gpuContext_) {
        gpuContext_->execute(*recording, pass);
    } else if (pixmap_) {
        CpuRenderer renderer(pixmap_.get());
        renderer.glyphCache_ = glyphCache_;
        renderer.execute(*recording, pass);
    }
}

std::shared_ptr<Image> Surface::makeSnapshot() const {
    if (pixmap_ && pixmap_->valid()) {
        return Image::MakeFromPixmap(*pixmap_);
    }
    if (gpuContext_) {
        return gpuContext_->makeSnapshot();
    }
    return nullptr;
}

bool Surface::isGPU() const {
    return gpuContext_ != nullptr && pixmap_ == nullptr;
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
    glyphCache_ = cache;
}

} // namespace ink
