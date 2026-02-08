#include "ink/device.hpp"

namespace ink {

void Device::beginFrame() {
    recorder_.reset();
    recording_.reset();
}

void Device::endFrame() {
    recording_ = recorder_.finish();
}

void Device::fillRect(Rect r, Color c) {
    recorder_.fillRect(r, c);
}

void Device::strokeRect(Rect r, Color c, f32 width) {
    recorder_.strokeRect(r, c, width);
}

void Device::drawLine(Point p1, Point p2, Color c, f32 width) {
    recorder_.drawLine(p1, p2, c, width);
}

void Device::drawPolyline(const Point* pts, i32 count, Color c, f32 width) {
    recorder_.drawPolyline(pts, count, c, width);
}

void Device::drawText(Point p, std::string_view text, Color c) {
    recorder_.drawText(p, text, c);
}

void Device::drawImage(std::shared_ptr<Image> image, f32 x, f32 y) {
    recorder_.drawImage(std::move(image), x, y);
}

void Device::setClipRect(Rect r) {
    recorder_.setClip(r);
}

void Device::resetClip() {
    recorder_.clearClip();
}

std::unique_ptr<Recording> Device::finishRecording() {
    return std::move(recording_);
}

}
