#include "ink/device.hpp"

namespace ink {

void Device::beginFrame() {
  recorder_.reset();
  recording_.reset();
}

void Device::endFrame() { recording_ = recorder_.finish(); }

void Device::fillRect(Rect r, const Paint &paint) {
  recorder_.fillRect(r, paint);
}

void Device::strokeRect(Rect r, const Paint &paint) {
  if (paint.pathEffect) {
    Path path;
    path.addRect(r);
    recorder_.strokePath(path, paint);
    return;
  }
  recorder_.strokeRect(r, paint);
}

void Device::drawLine(Point p1, Point p2, const Paint &paint) {
  if (paint.pathEffect) {
    Path path;
    path.moveTo(p1).lineTo(p2);
    recorder_.strokePath(path, paint);
    return;
  }
  recorder_.drawLine(p1, p2, paint);
}

void Device::drawPolyline(const Point *pts, i32 count, const Paint &paint) {
  if (paint.pathEffect && pts && count > 0) {
    Path path;
    path.moveTo(pts[0]);
    for (i32 i = 1; i < count; ++i)
      path.lineTo(pts[i]);
    recorder_.strokePath(path, paint);
    return;
  }
  recorder_.drawPolyline(pts, count, paint);
}

void Device::drawText(Point p, std::string_view text, const Paint &paint) {
  recorder_.drawText(p, text, paint);
}

void Device::drawImage(std::shared_ptr<Image> image, f32 x, f32 y) {
  recorder_.drawImage(std::move(image), x, y);
}

void Device::fillCircle(f32 cx, f32 cy, f32 radius, const Paint &paint) {
  recorder_.fillCircle(cx, cy, radius, paint);
}

void Device::strokeCircle(f32 cx, f32 cy, f32 radius, const Paint &paint) {
  if (paint.pathEffect) {
    Path path;
    path.addCircle(cx, cy, radius);
    recorder_.strokePath(path, paint);
    return;
  }
  recorder_.strokeCircle(cx, cy, radius, paint);
}

void Device::fillRoundRect(Rect r, f32 rx, f32 ry, const Paint &paint) {
  recorder_.fillRoundRect(r, rx, ry, paint);
}

void Device::strokeRoundRect(Rect r, f32 rx, f32 ry, const Paint &paint) {
  if (paint.pathEffect) {
    Path path;
    path.addRoundRect(r, rx, ry);
    recorder_.strokePath(path, paint);
    return;
  }
  recorder_.strokeRoundRect(r, rx, ry, paint);
}

void Device::drawPath(const Path &path, const Paint &paint) {
  if (paint.style == PaintStyle::Fill ||
      paint.style == PaintStyle::FillAndStroke) {
    recorder_.fillPath(path, paint);
  }
  if (paint.style == PaintStyle::Stroke ||
      paint.style == PaintStyle::FillAndStroke) {
    recorder_.strokePath(path, paint);
  }
}

void Device::drawImageRect(std::shared_ptr<Image> image, Rect src, Rect dst,
                           const Paint &paint) {
  recorder_.drawImageRect(std::move(image), src, dst, paint);
}

void Device::setClipRect(Rect r) { recorder_.setClip(r); }

void Device::setClipPath(const Path &path) { recorder_.setClipPath(path); }

void Device::resetClip() { recorder_.clearClip(); }

void Device::setTransform(const Matrix &m) { recorder_.setTransform(m); }

void Device::clearTransform() { recorder_.clearTransform(); }

std::unique_ptr<Recording> Device::finishRecording() {
  return std::move(recording_);
}

} // namespace ink
