#pragma once

#include "ink/types.hpp"
#include "ink/pixmap.hpp"
#include "ink/canvas.hpp"
#include "ink/context.hpp"
#include "ink/device.hpp"
#include "ink/pixel_data.hpp"
#include <memory>

namespace ink {

class GlyphCache;

class Surface {
public:
    // Auto surface - tries GPU first, falls back to CPU
    // Returns a valid surface, never nullptr (at minimum will return CPU raster)
    static std::unique_ptr<Surface> MakeAuto(i32 w, i32 h,
                                              PixelFormat fmt = PixelFormat::BGRA8888);

    // Raster (CPU) surface - allocates internal pixel buffer
    static std::unique_ptr<Surface> MakeRaster(i32 w, i32 h,
                                                PixelFormat fmt = PixelFormat::BGRA8888);

    // Raster (CPU) surface - wraps host-provided pixel buffer (zero-copy)
    static std::unique_ptr<Surface> MakeRasterDirect(const PixmapInfo& info, void* pixels);

    // GPU surface - host must have GL context current
    static std::unique_ptr<Surface> MakeGpu(std::unique_ptr<Context> context, i32 w, i32 h);

    // Recording surface - for offscreen command recording
    static std::unique_ptr<Surface> MakeRecording(i32 w, i32 h);

    ~Surface();

    Canvas* canvas() const { return canvas_.get(); }

    void resize(i32 w, i32 h);
    void beginFrame();
    void endFrame();
    void submit(const Recording& recording);
    void flush();

    // Pixel access (raster surfaces only, returns nullptr for GPU/recording)
    Pixmap* peekPixels();
    const Pixmap* peekPixels() const;

    // Get pixel data descriptor (convenience for host integration)
    PixelData getPixelData() const;

    // Check if this is a GPU-backed surface
    bool isGPU() const { return context_ != nullptr; }

    std::unique_ptr<Recording> takeRecording();
    void setGlyphCache(GlyphCache* cache);

private:
    Surface(std::unique_ptr<Device> device,
            std::unique_ptr<Context> context,
            std::unique_ptr<Pixmap> pixmap);

    std::unique_ptr<Device> device_;
    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<Context> context_;
    std::unique_ptr<Pixmap> pixmap_;
};

}
