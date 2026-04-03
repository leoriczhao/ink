#include "ink/device.hpp"

namespace ink {

void Device::beginFrame() {
    recorder_.reset();
    recording_.reset();
}

void Device::endFrame() {
    recording_ = recorder_.finish();
}

void Device::fillRect(Rect r, const Paint& paint) {
    recorder_.fillRect(r, paint);
}

void Device::strokeRect(Rect r, const Paint& paint) {
    recorder_.strokeRect(r, paint);
}

void Device::drawLine(Point p1, Point p2, const Paint& paint) {
    recorder_.drawLine(p1, p2, paint);
}

void Device::drawPolyline(const Point* pts, i32 count, const Paint& paint) {
    recorder_.drawPolyline(pts, count, paint);
}

void Device::drawText(Point p, std::string_view text, const Paint& paint) {
    recorder_.drawText(p, text, paint);
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

void Device::setTransform(const Matrix& m) {
    recorder_.setTransform(m);
}

void Device::clearTransform() {
    recorder_.clearTransform();
}

std::unique_ptr<Recording> Device::finishRecording() {
    return std::move(recording_);
}

}
