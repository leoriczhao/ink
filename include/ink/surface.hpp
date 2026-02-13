#pragma once

#include "ink/types.hpp"
#include "ink/pixmap.hpp"
#include "ink/canvas.hpp"
#include "ink/device.hpp"
#include "ink/backend.hpp"
#include "ink/pixel_data.hpp"
#include "ink/image.hpp"
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

    // GPU surface - creates internal Backend from GpuContext
    // Falls back to CPU if context is nullptr
    // Requires: #include <ink/gpu/gl/gl_context.hpp> to create GpuContext
    static std::unique_ptr<Surface> MakeGpu(std::shared_ptr<GpuContext> context,
                                            i32 w, i32 h,
                                            PixelFormat fmt = PixelFormat::BGRA8888);

    // Recording-only surface - no backend, just captures commands
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
    Surface(std::unique_ptr<Backend> backend,
            std::unique_ptr<Pixmap> pixmap);

    Device device_;
    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<Backend> backend_;
    std::unique_ptr<Pixmap> pixmap_;
};

}
