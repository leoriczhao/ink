#pragma once

/**
 * @file pixel_data.hpp
 * @brief Non-owning pixel data descriptor for host integration.
 */

#include "ink/types.hpp"
#include "ink/pixmap.hpp"
#include <cstdint>

namespace ink {

/// @brief Non-owning pixel data descriptor for host integration.
///
/// Does not own the pixel data — the caller must ensure the pixels remain
/// valid for the lifetime of this descriptor.
struct PixelData {
    const void* data = nullptr;  ///< Pointer to pixel data
    i32 width = 0;               ///< Width in pixels
    i32 height = 0;              ///< Height in pixels
    i32 rowBytes = 0;            ///< Bytes per row (stride)
    PixelFormat format = PixelFormat::BGRA8888; ///< Pixel format

    PixelData() = default;

    /// @brief Construct a PixelData descriptor.
    /// @param data_ Pointer to the raw pixel buffer.
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    /// @param stride Bytes per row.
    /// @param fmt Pixel format.
    PixelData(const void* data_, i32 w, i32 h, i32 stride, PixelFormat fmt)
        : data(data_), width(w), height(h), rowBytes(stride), format(fmt) {}

    /// @brief Check if the descriptor points to valid pixel data.
    /// @return True if data is non-null and dimensions are positive.
    bool isValid() const { return data != nullptr && width > 0 && height > 0 && rowBytes > 0; }

    /// @brief Compute the total size of the pixel data in bytes.
    /// @return Total byte count (height × rowBytes).
    std::int64_t sizeBytes() const { return std::int64_t(height) * rowBytes; }
};

} // namespace ink
