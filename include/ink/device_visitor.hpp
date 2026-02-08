#pragma once
#include "ink/draw_op_visitor.hpp"
#include "ink/device.hpp"

namespace ink {

class DeviceVisitor : public DrawOpVisitor {
    Device* device_;
public:
    explicit DeviceVisitor(Device* device);

    void visitFillRect(Rect r, Color c) override;
    void visitStrokeRect(Rect r, Color c, f32 width) override;
    void visitLine(Point p1, Point p2, Color c, f32 width) override;
    void visitPolyline(const Point* pts, i32 count, Color c, f32 width) override;
    void visitText(Point p, const char* text, u32 len, Color c) override;
    void visitSetClip(Rect r) override;
    void visitClearClip() override;
};

}
