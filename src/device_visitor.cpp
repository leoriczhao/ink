#include "ink/device_visitor.hpp"

namespace ink {

DeviceVisitor::DeviceVisitor(Device* device) : device_(device) {}

void DeviceVisitor::visitFillRect(Rect r, Color c) {
    device_->fillRect(r, c);
}

void DeviceVisitor::visitStrokeRect(Rect r, Color c, f32 width) {
    device_->strokeRect(r, c, width);
}

void DeviceVisitor::visitLine(Point p1, Point p2, Color c, f32 width) {
    device_->drawLine(p1, p2, c, width);
}

void DeviceVisitor::visitPolyline(const Point* pts, i32 count, Color c, f32 width) {
    device_->drawPolyline(pts, count, c, width);
}

void DeviceVisitor::visitText(Point p, const char* text, u32 len, Color c) {
    device_->drawText(p, std::string_view(text, len), c);
}

void DeviceVisitor::visitSetClip(Rect r) {
    device_->setClipRect(r);
}

void DeviceVisitor::visitClearClip() {
    device_->resetClip();
}

}
