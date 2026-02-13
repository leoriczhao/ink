#pragma once

#include "backend.hpp"
#include "ink/pixmap.hpp"

namespace ink {

class GlyphCache;
class Image;

/**
 * CpuBackend - Software rasterization backend.
 *
 * Executes draw operations by writing pixels directly into a Pixmap.
 * This is the CPU equivalent of what a GpuBackend would do with GL calls.
 */
class CpuBackend : public Backend {
public:
    explicit CpuBackend(Pixmap* target);

    void execute(const Recording& recording, const DrawPass& pass) override;
    void beginFrame() override;
    void endFrame() override;
    void resize(i32 w, i32 h) override;
    void setGlyphCache(GlyphCache* cache) override { glyphCache_ = cache; }

private:
    Pixmap* target_ = nullptr;
    GlyphCache* glyphCache_ = nullptr;

    Rect clipRect_ = {};
    bool hasClip_ = false;

    // Primitive rendering
    void blendPixel(i32 x, i32 y, Color c);
    void drawHLine(i32 x1, i32 x2, i32 y, Color c);
    void drawLineImpl(i32 x1, i32 y1, i32 x2, i32 y2, Color c);
    void drawImageImpl(const Image* image, f32 x, f32 y);

    bool isClipped(i32 x, i32 y) const;
    Rect effectiveClip() const;

    // Execute individual operations
    void execFillRect(Rect r, Color c);
    void execStrokeRect(Rect r, Color c, f32 width);
    void execLine(Point p1, Point p2, Color c, f32 width);
    void execPolyline(const Point* pts, i32 count, Color c, f32 width);
    void execText(Point p, const char* text, u32 len, Color c);
    void execDrawImage(const Image* image, f32 x, f32 y);
    void execSetClip(Rect r);
    void execClearClip();
};

} // namespace ink
