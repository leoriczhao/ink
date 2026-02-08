#pragma once

#include "ink/types.hpp"
#include "ink/recording.hpp"
#include <memory>

namespace ink {

class GlyphCache;

class Device {
public:
    virtual ~Device() = default;

    virtual void resize(i32 w, i32 h) = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;

    virtual void fillRect(Rect r, Color c) = 0;
    virtual void strokeRect(Rect r, Color c, f32 width = 1.0f) = 0;
    virtual void drawLine(Point p1, Point p2, Color c, f32 width = 1.0f) = 0;
    virtual void drawPolyline(const Point* pts, i32 count, Color c, f32 width = 1.0f) = 0;
    virtual void drawText(Point p, std::string_view text, Color c) = 0;

    virtual void setClipRect(Rect r) = 0;
    virtual void resetClip() = 0;

    virtual void setGlyphCache(GlyphCache* cache) { (void)cache; }
    virtual std::unique_ptr<Recording> finishRecording() { return nullptr; }
};

class GpuDevice : public Device {
public:
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

    std::unique_ptr<Recording> finishRecording() override;

private:
    Recorder recorder_;
    std::unique_ptr<Recording> recording_;
};

}
