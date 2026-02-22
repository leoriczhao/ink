#pragma once

/**
 * @file pixmap.hpp
 * @brief Pixel format, pixel buffer descriptor, and owning/non-owning pixel buffer.
 */

#include "ink/types.hpp"
#include <cstdlib>
#include <cstring>

namespace ink {

/// @brief Pixel format enumeration.
enum class PixelFormat {
    RGBA8888,  ///< Red-Green-Blue-Alpha, 8 bits each.
    BGRA8888,  ///< Blue-Green-Red-Alpha, 8 bits each (native on many platforms).
};

/// @brief Descriptor for pixel buffer dimensions, stride, and format.
struct PixmapInfo {
    i32 width = 0;   ///< Width in pixels.
    i32 height = 0;  ///< Height in pixels.
    i32 stride = 0;  ///< Bytes per row.
    PixelFormat format = PixelFormat::RGBA8888; ///< Pixel format.

    /// @brief Get bytes per pixel (always 4 for current formats).
    /// @return Bytes per pixel.
    i32 bytesPerPixel() const { return 4; }

    /// @brief Compute total byte size of the pixel buffer.
    /// @return stride × height.
    i32 computeByteSize() const { return stride * height; }

    /// @brief Create a PixmapInfo with the given dimensions and format.
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    /// @param fmt Pixel format.
    /// @return A new PixmapInfo.
    static PixmapInfo Make(i32 w, i32 h, PixelFormat fmt) {
        PixmapInfo info;
        info.width = w;
        info.height = h;
        info.format = fmt;
        info.stride = w * 4;
        return info;
    }

    /// @brief Create a PixmapInfo with RGBA8888 format.
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    /// @return A new PixmapInfo with RGBA8888 format.
    static PixmapInfo MakeRGBA(i32 w, i32 h) { return Make(w, h, PixelFormat::RGBA8888); }

    /// @brief Create a PixmapInfo with BGRA8888 format.
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    /// @return A new PixmapInfo with BGRA8888 format.
    static PixmapInfo MakeBGRA(i32 w, i32 h) { return Make(w, h, PixelFormat::BGRA8888); }
};

/// @brief Owning or non-owning pixel buffer.
///
/// Use Alloc() to create an owned buffer, or Wrap() to reference external memory.
class Pixmap {
public:
    /// @brief Allocate a new pixel buffer described by info.
    /// @param info Dimensions, stride, and format of the buffer.
    /// @return A new Pixmap that owns its pixel data.
    static Pixmap Alloc(const PixmapInfo& info);

    /// @brief Wrap existing pixel memory (caller keeps ownership).
    /// @param info Dimensions, stride, and format of the buffer.
    /// @param pixels Pointer to the external pixel data.
    /// @return A non-owning Pixmap.
    static Pixmap Wrap(const PixmapInfo& info, void* pixels);

    Pixmap() = default;
    ~Pixmap();

    Pixmap(Pixmap&& other) noexcept;
    Pixmap& operator=(Pixmap&& other) noexcept;

    Pixmap(const Pixmap&) = delete;
    Pixmap& operator=(const Pixmap&) = delete;

    /// @brief Get raw pixel pointer (mutable).
    void* addr() { return pixels_; }
    /// @brief Get raw pixel pointer (const).
    const void* addr() const { return pixels_; }
    /// @brief Get pixel pointer as u8* (mutable).
    u8* addr8() { return static_cast<u8*>(pixels_); }
    /// @brief Get pixel pointer as u8* (const).
    const u8* addr8() const { return static_cast<const u8*>(pixels_); }
    /// @brief Get pixel pointer as u32* (mutable).
    u32* addr32() { return static_cast<u32*>(pixels_); }
    /// @brief Get pixel pointer as u32* (const).
    const u32* addr32() const { return static_cast<const u32*>(pixels_); }

    /// @brief Get the pixmap info descriptor.
    const PixmapInfo& info() const { return info_; }
    /// @brief Get width in pixels.
    i32 width() const { return info_.width; }
    /// @brief Get height in pixels.
    i32 height() const { return info_.height; }
    /// @brief Get row stride in bytes.
    i32 stride() const { return info_.stride; }
    /// @brief Get pixel format.
    PixelFormat format() const { return info_.format; }

    /// @brief Check if the pixmap has valid pixel data.
    /// @return True if pixels are non-null and dimensions are positive.
    bool valid() const { return pixels_ != nullptr && info_.width > 0 && info_.height > 0; }

    /// @brief Get pointer to the start of a specific row (mutable).
    /// @param y Row index.
    void* rowAddr(i32 y) { return addr8() + y * info_.stride; }
    /// @brief Get pointer to the start of a specific row (const).
    /// @param y Row index.
    const void* rowAddr(i32 y) const { return addr8() + y * info_.stride; }

    /// @brief Fill the entire buffer with a color.
    /// @param c The fill color.
    void clear(Color c);

    /// @brief Release pixel data and reset to empty state.
    void reset();

    /// @brief Reallocate the buffer with new dimensions/format.
    /// @param info New pixmap descriptor.
    void reallocate(const PixmapInfo& info);

private:
    Pixmap(const PixmapInfo& info, void* pixels, bool ownsPixels);

    PixmapInfo info_;
    void* pixels_ = nullptr;
    bool ownsPixels_ = false;
};

}
