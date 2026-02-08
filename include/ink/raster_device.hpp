#pragma once

#include "ink/device.hpp"
#include "ink/pixmap.hpp"

namespace ink {

class GlyphCache;

class SoftwareRasterDevice : public Device {
public:
    explicit SoftwareRasterDevice(Pixmap* target);
    ~SoftwareRasterDevice() override = default;

    void resize(i32 w, i32 h) override;
    void beginFrame() override;
    void endFrame() override;

    void fillRect(Rect r, Color c) override;
    void strokeRect(Rect r, Color c, f32 width) override;
    void drawLine(Point p1, Point p2, Color c, f32 width) override;
    void drawPolyline(const Point* pts, i32 count, Color c, f32 width) override;
    void drawText(Point p, std::string_view text, Color c) override;

    void setClipRect(Rect r) override;
    void resetClip() override;

    void setGlyphCache(GlyphCache* cache) override { glyphCache_ = cache; }

private:
    Pixmap* target_ = nullptr;
    GlyphCache* glyphCache_ = nullptr;

    Rect clipRect_ = {};
    bool hasClip_ = false;

    void blendPixel(i32 x, i32 y, Color c);
    void drawHLine(i32 x1, i32 x2, i32 y, Color c);
    void drawLineImpl(i32 x1, i32 y1, i32 x2, i32 y2, Color c);

    bool isClipped(i32 x, i32 y) const;
    Rect effectiveClip() const;
};

}
