#include "ink/image.hpp"
#include <cstring>
#include <atomic>

namespace ink {

namespace {
std::atomic<u64> gNextImageId{1};
}

u64 Image::nextImageId() {
    return gNextImageId.fetch_add(1, std::memory_order_relaxed);
}

Image::Image(const PixmapInfo& info, const void* pixels, std::unique_ptr<Pixmap> owned)
    : id_(nextImageId()),
      storageType_(StorageType::CpuPixmap),
      info_(info),
      pixels_(pixels),
      ownedPixmap_(std::move(owned)) {
}

Image::Image(const PixmapInfo& info, u32 textureId, std::shared_ptr<void> lifetimeToken)
    : id_(nextImageId()),
      storageType_(StorageType::GpuTexture),
      info_(info),
      glTextureId_(textureId),
      gpuLifetimeToken_(std::move(lifetimeToken)) {
}

std::shared_ptr<Image> Image::MakeFromPixmap(const Pixmap& src) {
    if (!src.valid()) return nullptr;

    // Copy the pixel data
    auto copy = std::make_unique<Pixmap>(Pixmap::Alloc(src.info()));
    if (!copy->valid()) return nullptr;

    std::memcpy(copy->addr(), src.addr(), src.info().computeByteSize());

    const void* pixels = copy->addr();
    PixmapInfo info = copy->info();
    return std::shared_ptr<Image>(new Image(info, pixels, std::move(copy)));
}

std::shared_ptr<Image> Image::MakeFromPixmapNoCopy(const Pixmap& src) {
    if (!src.valid()) return nullptr;
    return std::shared_ptr<Image>(new Image(src.info(), src.addr(), nullptr));
}

std::shared_ptr<Image> Image::MakeFromGLTexture(u32 textureId,
                                                i32 width,
                                                i32 height,
                                                PixelFormat fmt,
                                                std::shared_ptr<void> lifetimeToken) {
    if (textureId == 0 || width <= 0 || height <= 0) return nullptr;
    PixmapInfo info = PixmapInfo::Make(width, height, fmt);
    return std::shared_ptr<Image>(new Image(info, textureId, std::move(lifetimeToken)));
}

} // namespace ink
