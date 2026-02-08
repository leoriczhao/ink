#pragma once

#include "ink/types.hpp"
#include "ink/pixmap.hpp"
#include <memory>

namespace ink {

/**
 * Image - An immutable snapshot of pixel data.
 *
 * Images are created from Surfaces via makeSnapshot() and can be drawn
 * onto any Surface via canvas->drawImage(). This is the mechanism for
 * compositing multiple surfaces together.
 *
 * Currently only CPU-backed images (Pixmap) are supported.
 * Future: GPU-backed images (texture handle) will be added.
 */
class Image {
public:
    // Create an image by copying pixel data from a Pixmap
    static std::shared_ptr<Image> MakeFromPixmap(const Pixmap& src);

    // Create an image wrapping existing pixel data (caller must keep data alive)
    static std::shared_ptr<Image> MakeFromPixmapNoCopy(const Pixmap& src);

    i32 width() const { return info_.width; }
    i32 height() const { return info_.height; }
    PixelFormat format() const { return info_.format; }
    const PixmapInfo& info() const { return info_; }

    const void* pixels() const { return pixels_; }
    const u32* pixels32() const { return static_cast<const u32*>(pixels_); }
    i32 stride() const { return info_.stride; }

    bool valid() const { return pixels_ != nullptr && info_.width > 0 && info_.height > 0; }

private:
    Image(const PixmapInfo& info, const void* pixels, std::unique_ptr<Pixmap> owned);

    PixmapInfo info_;
    const void* pixels_ = nullptr;
    std::unique_ptr<Pixmap> ownedPixmap_;  // non-null if we own the pixel data
};

} // namespace ink
