#include "ink/image.hpp"
#include <cstring>

namespace ink {

Image::Image(const PixmapInfo& info, const void* pixels, std::unique_ptr<Pixmap> owned)
    : info_(info), pixels_(pixels), ownedPixmap_(std::move(owned)) {
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

} // namespace ink
