#include "ink/device.hpp"

namespace ink {

void GpuDevice::resize(i32, i32) {
}

void GpuDevice::beginFrame() {
    recorder_.reset();
    recording_.reset();
}

void GpuDevice::endFrame() {
    recording_ = recorder_.finish();
}

void GpuDevice::fillRect(Rect r, Color c) {
    recorder_.fillRect(r, c);
}

void GpuDevice::strokeRect(Rect r, Color c, f32 width) {
    recorder_.strokeRect(r, c, width);
}

void GpuDevice::drawLine(Point p1, Point p2, Color c, f32 width) {
    recorder_.drawLine(p1, p2, c, width);
}

void GpuDevice::drawPolyline(const Point* pts, i32 count, Color c, f32 width) {
    recorder_.drawPolyline(pts, count, c, width);
}

void GpuDevice::drawText(Point p, std::string_view text, Color c) {
    recorder_.drawText(p, text, c);
}

void GpuDevice::setClipRect(Rect r) {
    recorder_.setClip(r);
}

void GpuDevice::resetClip() {
    recorder_.clearClip();
}

std::unique_ptr<Recording> GpuDevice::finishRecording() {
    return std::move(recording_);
}

}
