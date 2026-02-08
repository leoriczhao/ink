#pragma once

#include "ink/types.hpp"
#include "ink/pixmap.hpp"
#include <cstdint>

namespace ink {

// Simple pixel data descriptor for host integration
// Does not own the pixel data - caller must ensure pixels remain valid
struct PixelData {
    const void* data = nullptr;  // Pointer to pixel data
    i32 width = 0;
    i32 height = 0;
    i32 rowBytes = 0;             // Bytes per row (stride)
    PixelFormat format = PixelFormat::BGRA8888;

    PixelData() = default;

    PixelData(const void* data_, i32 w, i32 h, i32 stride, PixelFormat fmt)
        : data(data_), width(w), height(h), rowBytes(stride), format(fmt) {}

    // Check if data is valid
    bool isValid() const { return data != nullptr && width > 0 && height > 0 && rowBytes > 0; }

    // Get total size in bytes
    std::int64_t sizeBytes() const { return std::int64_t(height) * rowBytes; }
};

} // namespace ink
