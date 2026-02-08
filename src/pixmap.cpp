#include "ink/pixmap.hpp"

namespace ink {

Pixmap::Pixmap(const PixmapInfo& info, void* pixels, bool ownsPixels)
    : info_(info), pixels_(pixels), ownsPixels_(ownsPixels) {
}

Pixmap::~Pixmap() {
    reset();
}

Pixmap::Pixmap(Pixmap&& other) noexcept
    : info_(other.info_), pixels_(other.pixels_), ownsPixels_(other.ownsPixels_) {
    other.pixels_ = nullptr;
    other.ownsPixels_ = false;
    other.info_ = {};
}

Pixmap& Pixmap::operator=(Pixmap&& other) noexcept {
    if (this != &other) {
        reset();
        info_ = other.info_;
        pixels_ = other.pixels_;
        ownsPixels_ = other.ownsPixels_;
        other.pixels_ = nullptr;
        other.ownsPixels_ = false;
        other.info_ = {};
    }
    return *this;
}

Pixmap Pixmap::Alloc(const PixmapInfo& info) {
    i32 size = info.computeByteSize();
    if (size <= 0) return {};
    void* pixels = std::calloc(1, size);
    if (!pixels) return {};
    return Pixmap(info, pixels, true);
}

Pixmap Pixmap::Wrap(const PixmapInfo& info, void* pixels) {
    if (!pixels) return {};
    return Pixmap(info, pixels, false);
}

void Pixmap::clear(Color c) {
    if (!pixels_) return;

    u32 pixel;
    if (info_.format == PixelFormat::BGRA8888) {
        pixel = (u32(c.a) << 24) | (u32(c.r) << 16) | (u32(c.g) << 8) | u32(c.b);
    } else {
        pixel = (u32(c.a) << 24) | (u32(c.b) << 16) | (u32(c.g) << 8) | u32(c.r);
    }

    u32* p = addr32();
    i32 count = info_.width * info_.height;
    for (i32 i = 0; i < count; ++i) {
        p[i] = pixel;
    }
}

void Pixmap::reset() {
    if (ownsPixels_ && pixels_) {
        std::free(pixels_);
    }
    pixels_ = nullptr;
    ownsPixels_ = false;
    info_ = {};
}

void Pixmap::reallocate(const PixmapInfo& info) {
    reset();
    *this = Alloc(info);
}

}
