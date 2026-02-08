#include "ink/canvas.hpp"
#include "ink/device.hpp"
#include <algorithm>

namespace ink {

Canvas::Canvas(Device* device) : device_(device) {}

void Canvas::fillRect(Rect r, Color c) {
    device_->fillRect(r, c);
}

void Canvas::strokeRect(Rect r, Color c, f32 width) {
    device_->strokeRect(r, c, width);
}

void Canvas::drawLine(Point p1, Point p2, Color c, f32 width) {
    device_->drawLine(p1, p2, c, width);
}

void Canvas::drawPolyline(const Point* pts, i32 count, Color c, f32 width) {
    device_->drawPolyline(pts, count, c, width);
}

void Canvas::drawText(Point p, std::string_view text, Color c) {
    device_->drawText(p, text, c);
}

void Canvas::drawImage(std::shared_ptr<Image> image, f32 x, f32 y) {
    device_->drawImage(std::move(image), x, y);
}

void Canvas::save() {
    stack_.push_back(current_);
}

void Canvas::restore() {
    if (stack_.empty()) return;
    current_ = stack_.back();
    stack_.pop_back();
    applyClip();
}

void Canvas::clipRect(Rect r) {
    if (current_.hasClip) {
        f32 x0 = std::max(current_.clip.x, r.x);
        f32 y0 = std::max(current_.clip.y, r.y);
        f32 x1 = std::min(current_.clip.x + current_.clip.w, r.x + r.w);
        f32 y1 = std::min(current_.clip.y + current_.clip.h, r.y + r.h);
        if (x1 < x0 || y1 < y0) {
            current_.clip = {0, 0, 0, 0};
        } else {
            current_.clip = {x0, y0, x1 - x0, y1 - y0};
        }
    } else {
        current_.clip = r;
        current_.hasClip = true;
    }
    applyClip();
}

void Canvas::applyClip() {
    if (current_.hasClip) {
        device_->setClipRect(current_.clip);
    } else {
        device_->resetClip();
    }
}

}
