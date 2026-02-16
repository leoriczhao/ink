#pragma once

#include "ink/types.hpp"
#include <cstdlib>
#include <cstring>

namespace ink {

enum class PixelFormat {
    RGBA8888,
    BGRA8888,
};

struct PixmapInfo {
    i32 width = 0;
    i32 height = 0;
    i32 stride = 0;
    PixelFormat format = PixelFormat::RGBA8888;

    i32 bytesPerPixel() const { return 4; }
    i32 computeByteSize() const { return stride * height; }

    static PixmapInfo Make(i32 w, i32 h, PixelFormat fmt) {
        PixmapInfo info;
        info.width = w;
        info.height = h;
        info.format = fmt;
        info.stride = w * 4;
        return info;
    }

    static PixmapInfo MakeRGBA(i32 w, i32 h) { return Make(w, h, PixelFormat::RGBA8888); }
    static PixmapInfo MakeBGRA(i32 w, i32 h) { return Make(w, h, PixelFormat::BGRA8888); }
};

class Pixmap {
public:
    static Pixmap Alloc(const PixmapInfo& info);
    static Pixmap Wrap(const PixmapInfo& info, void* pixels);

    Pixmap() = default;
    ~Pixmap();

    Pixmap(Pixmap&& other) noexcept;
    Pixmap& operator=(Pixmap&& other) noexcept;

    Pixmap(const Pixmap&) = delete;
    Pixmap& operator=(const Pixmap&) = delete;

    void* addr() { return pixels_; }
    const void* addr() const { return pixels_; }
    u8* addr8() { return static_cast<u8*>(pixels_); }
    const u8* addr8() const { return static_cast<const u8*>(pixels_); }
    u32* addr32() { return static_cast<u32*>(pixels_); }
    const u32* addr32() const { return static_cast<const u32*>(pixels_); }

    const PixmapInfo& info() const { return info_; }
    i32 width() const { return info_.width; }
    i32 height() const { return info_.height; }
    i32 stride() const { return info_.stride; }
    PixelFormat format() const { return info_.format; }

    bool valid() const { return pixels_ != nullptr && info_.width > 0 && info_.height > 0; }

    void* rowAddr(i32 y) { return addr8() + y * info_.stride; }
    const void* rowAddr(i32 y) const { return addr8() + y * info_.stride; }

    void clear(Color c);

    void reset();
    void reallocate(const PixmapInfo& info);

private:
    Pixmap(const PixmapInfo& info, void* pixels, bool ownsPixels);

    PixmapInfo info_;
    void* pixels_ = nullptr;
    bool ownsPixels_ = false;
};

}
