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
 */
class Image {
public:
    enum class StorageType : u8 {
        CpuPixmap,
        GpuTexture
    };

    // Create an image by copying pixel data from a Pixmap
    static std::shared_ptr<Image> MakeFromPixmap(const Pixmap& src);

    // Create an image wrapping existing pixel data (caller must keep data alive)
    static std::shared_ptr<Image> MakeFromPixmapNoCopy(const Pixmap& src);

    // Create an image from an existing GPU texture handle.
    // The optional lifetime token can hold a custom deleter to release the texture.
    static std::shared_ptr<Image> MakeFromGLTexture(u32 textureId,
                                                    i32 width,
                                                    i32 height,
                                                    PixelFormat fmt = PixelFormat::RGBA8888,
                                                    std::shared_ptr<void> lifetimeToken = nullptr);

    i32 width() const { return info_.width; }
    i32 height() const { return info_.height; }
    PixelFormat format() const { return info_.format; }
    const PixmapInfo& info() const { return info_; }

    const void* pixels() const { return pixels_; }
    const u32* pixels32() const { return static_cast<const u32*>(pixels_); }
    i32 stride() const { return info_.stride; }

    StorageType storageType() const { return storageType_; }
    bool isCpuBacked() const { return storageType_ == StorageType::CpuPixmap; }
    bool isGpuBacked() const { return storageType_ == StorageType::GpuTexture; }

    // Returns GL texture id for GPU-backed images; 0 otherwise.
    u32 glTextureId() const { return glTextureId_; }

    // Stable identity used for backend caches.
    u64 uniqueId() const { return id_; }

    bool valid() const {
        if (info_.width <= 0 || info_.height <= 0) return false;
        if (storageType_ == StorageType::CpuPixmap) return pixels_ != nullptr;
        return glTextureId_ != 0;
    }

private:
    static u64 nextImageId();

    Image(const PixmapInfo& info, const void* pixels, std::unique_ptr<Pixmap> owned);
    Image(const PixmapInfo& info, u32 textureId, std::shared_ptr<void> lifetimeToken);

    u64 id_ = 0;
    StorageType storageType_ = StorageType::CpuPixmap;
    PixmapInfo info_;
    const void* pixels_ = nullptr;
    std::unique_ptr<Pixmap> ownedPixmap_;  // non-null if we own the pixel data
    u32 glTextureId_ = 0;
    std::shared_ptr<void> gpuLifetimeToken_;
};

} // namespace ink
