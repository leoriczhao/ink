#pragma once

/**
 * @file surface.hpp
 * @brief Top-level rendering target (CPU raster, GPU, or recording-only).
 */

#include "ink/types.hpp"
#include "ink/pixmap.hpp"
#include "ink/canvas.hpp"
#include "ink/device.hpp"
#include "ink/pixel_data.hpp"
#include "ink/image.hpp"
#include "ink/renderer.hpp"
#include <memory>

namespace ink {

class GlyphCache;
class GpuContext;

/// @brief Top-level rendering target.
///
/// A Surface owns a Canvas, a Device, and a Renderer. The typical frame
/// cycle is: beginFrame() → draw via canvas() → endFrame() → flush().
///
/// @note The caller must ensure the GL context is current when using a
///       GPU-backed surface.
class Surface {
public:
    /// @brief Create a CPU raster surface that allocates its own pixel buffer.
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    /// @param fmt Pixel format (default BGRA8888).
    /// @return Unique pointer to the new Surface.
    static std::unique_ptr<Surface> MakeRaster(i32 w, i32 h,
                                                PixelFormat fmt = PixelFormat::BGRA8888);

    /// @brief Create a CPU raster surface wrapping a caller-owned pixel buffer (zero-copy).
    /// @param info Pixmap descriptor for the external buffer.
    /// @param pixels Pointer to the caller-owned pixel data.
    /// @return Unique pointer to the new Surface.
    static std::unique_ptr<Surface> MakeRasterDirect(const PixmapInfo& info, void* pixels);

    /// @brief Create a GPU surface. Falls back to CPU if context is null or invalid.
    /// @param context Shared pointer to the GPU context.
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    /// @param fmt Pixel format (default BGRA8888).
    /// @return Unique pointer to the new Surface.
    static std::unique_ptr<Surface> MakeGpu(const std::shared_ptr<GpuContext>& context,
                                            i32 w, i32 h,
                                            PixelFormat fmt = PixelFormat::BGRA8888);

    /// @brief Create a recording-only surface (no rendering, just captures commands).
    /// @param w Width in pixels.
    /// @param h Height in pixels.
    /// @return Unique pointer to the new Surface.
    static std::unique_ptr<Surface> MakeRecording(i32 w, i32 h);

    ~Surface();

    /// @brief Get the Canvas used for drawing on this surface.
    /// @return Pointer to the Canvas.
    Canvas* canvas() const { return canvas_.get(); }

    /// @brief Resize the surface to new dimensions.
    /// @param w New width in pixels.
    /// @param h New height in pixels.
    void resize(i32 w, i32 h);

    /// @brief Begin a new frame, clearing to the specified color.
    /// @param clearColor The color to clear the render target to (default: opaque black).
    void beginFrame(Color clearColor = {0, 0, 0, 255});

    /// @brief End the current frame (finishes recording).
    void endFrame();

    /// @brief Execute recorded commands and submit to the backend.
    void flush();

    /// @brief Create an immutable snapshot of current surface contents.
    /// @return Shared pointer to the snapshot Image.
    std::shared_ptr<Image> makeSnapshot() const;

    /// @brief Direct pixel access (raster surfaces only).
    /// @return Pointer to the underlying Pixmap, or nullptr for GPU/recording surfaces.
    Pixmap* peekPixels();

    /// @copydoc peekPixels()
    const Pixmap* peekPixels() const;

    /// @brief Get a non-owning pixel data descriptor for host integration.
    /// @return A PixelData descriptor.
    PixelData getPixelData() const;

    /// @brief Check if this is a GPU-backed surface.
    /// @return True if GPU-backed.
    bool isGPU() const;

    /// @brief Take ownership of the current recording (for recording surfaces).
    /// @return Unique pointer to the Recording.
    std::unique_ptr<Recording> takeRecording();

    /// @brief Set the glyph cache used for text rendering.
    /// @param cache Pointer to the glyph cache.
    void setGlyphCache(GlyphCache* cache);

private:
    Surface(std::shared_ptr<Renderer> renderer, std::unique_ptr<Pixmap> pixmap);

    Device device_;
    std::unique_ptr<Canvas> canvas_;
    std::shared_ptr<Renderer> renderer_;
    std::unique_ptr<Pixmap> pixmap_;
    GlyphCache* glyphCache_ = nullptr;
};

} // namespace ink
