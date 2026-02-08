#include "ink/surface.hpp"
#include "ink/raster_device.hpp"

namespace ink {

std::unique_ptr<Surface> Surface::MakeRaster(i32 w, i32 h, PixelFormat fmt) {
    auto pixmap = std::make_unique<Pixmap>(Pixmap::Alloc(PixmapInfo::Make(w, h, fmt)));
    if (!pixmap->valid()) return nullptr;

    auto device = std::make_unique<SoftwareRasterDevice>(pixmap.get());
    return std::unique_ptr<Surface>(new Surface(std::move(device), nullptr, std::move(pixmap)));
}

std::unique_ptr<Surface> Surface::MakeRasterDirect(const PixmapInfo& info, void* pixels) {
    auto pixmap = std::make_unique<Pixmap>(Pixmap::Wrap(info, pixels));
    if (!pixmap->valid()) return nullptr;

    auto device = std::make_unique<SoftwareRasterDevice>(pixmap.get());
    return std::unique_ptr<Surface>(new Surface(std::move(device), nullptr, std::move(pixmap)));
}

}
