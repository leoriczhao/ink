#include "ink/surface.hpp"
#include "ink/canvas.hpp"
#include "ink/context.hpp"
#include "ink/device.hpp"
#include "ink/device_visitor.hpp"

namespace ink {

std::unique_ptr<Surface> Surface::MakeAuto(i32 w, i32 h, PixelFormat fmt) {
#if WAVEFORM_HAS_GL
    // Try GPU first
    try {
        auto ctx = Context::MakeGL();
        if (ctx && ctx->init(w, h)) {
            return Surface::MakeGpu(std::move(ctx), w, h);
        }
    } catch (...) {
        // Fall through to CPU
    }
#endif
    // Fall back to CPU raster
    return Surface::MakeRaster(w, h, fmt);
}

Surface::Surface(std::unique_ptr<Device> device,
                 std::unique_ptr<Context> context,
                 std::unique_ptr<Pixmap> pixmap)
    : device_(std::move(device)), context_(std::move(context)),
      pixmap_(std::move(pixmap)) {
    canvas_ = std::make_unique<Canvas>(device_.get());
}

Surface::~Surface() = default;

void Surface::resize(i32 w, i32 h) {
    if (pixmap_) {
        PixmapInfo info = PixmapInfo::Make(w, h, pixmap_->format());
        pixmap_->reallocate(info);
    }
    device_->resize(w, h);
    if (context_) {
        context_->resize(w, h);
    }
}

void Surface::beginFrame() {
    device_->beginFrame();
    if (context_) {
        context_->beginFrame();
    }
}

void Surface::endFrame() {
    device_->endFrame();
}

void Surface::submit(const Recording& recording) {
    if (context_) {
        context_->submit(recording);
        return;
    }
    DeviceVisitor visitor(device_.get());
    recording.accept(visitor);
}

void Surface::flush() {
    if (context_) {
        auto recording = device_->finishRecording();
        if (recording) {
            context_->submit(*recording);
        }
        context_->flush();
    }
    // Raster surfaces: no-op, pixels are already in the Pixmap
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
    return device_->finishRecording();
}

void Surface::setGlyphCache(GlyphCache* cache) {
    if (context_) {
        context_->setGlyphCache(cache);
    }
    device_->setGlyphCache(cache);
}

}
