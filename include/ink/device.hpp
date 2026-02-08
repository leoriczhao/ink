#pragma once

#include "ink/types.hpp"
#include "ink/recording.hpp"
#include <memory>

namespace ink {

class GlyphCache;
class Image;

/**
 * Device - The single recording device.
 *
 * Device converts Canvas high-level commands into low-level Recording ops.
 * All Surfaces use the same Device - it always records, never draws directly.
 * The actual rendering is done by a Backend that consumes the Recording.
 */
class Device {
public:
    Device() = default;
    ~Device() = default;

    void beginFrame();
    void endFrame();

    void fillRect(Rect r, Color c);
    void strokeRect(Rect r, Color c, f32 width = 1.0f);
    void drawLine(Point p1, Point p2, Color c, f32 width = 1.0f);
    void drawPolyline(const Point* pts, i32 count, Color c, f32 width = 1.0f);
    void drawText(Point p, std::string_view text, Color c);
    void drawImage(std::shared_ptr<Image> image, f32 x, f32 y);

    void setClipRect(Rect r);
    void resetClip();

    std::unique_ptr<Recording> finishRecording();

private:
    Recorder recorder_;
    std::unique_ptr<Recording> recording_;
};

}
