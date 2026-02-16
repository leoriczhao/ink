#pragma once

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

class Surface {
public:
    // CPU raster surface - allocates internal pixel buffer
    static std::unique_ptr<Surface> MakeRaster(i32 w, i32 h,
                                                PixelFormat fmt = PixelFormat::BGRA8888);

    // CPU raster surface - wraps host-provided pixel buffer (zero-copy)
    static std::unique_ptr<Surface> MakeRasterDirect(const PixmapInfo& info, void* pixels);

    // GPU surface - uses GpuContext for rendering
    // Falls back to CPU if context is nullptr or invalid
    static std::unique_ptr<Surface> MakeGpu(const std::shared_ptr<GpuContext>& context,
                                            i32 w, i32 h,
                                            PixelFormat fmt = PixelFormat::BGRA8888);

    // Recording-only surface - no rendering, just captures commands
    static std::unique_ptr<Surface> MakeRecording(i32 w, i32 h);

    ~Surface();

    Canvas* canvas() const { return canvas_.get(); }

    void resize(i32 w, i32 h);
    void beginFrame();
    void endFrame();
    void flush();

    // Create an immutable snapshot of current surface contents
    std::shared_ptr<Image> makeSnapshot() const;

    // Pixel access (raster surfaces only, returns nullptr for GPU/recording)
    Pixmap* peekPixels();
    const Pixmap* peekPixels() const;

    // Get pixel data descriptor (convenience for host integration)
    PixelData getPixelData() const;

    // Check if this is a GPU-backed surface
    bool isGPU() const;

    // Get the recording (for recording surfaces, or for inspection)
    std::unique_ptr<Recording> takeRecording();

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
