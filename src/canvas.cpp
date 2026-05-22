#include "ink/canvas.hpp"
#include "ink/device.hpp"
#include "ink/image.hpp"
#include <algorithm>

namespace ink {

Canvas::Canvas(Device *device) : device_(device) {}

void Canvas::fillRect(Rect r, Color c) { device_->fillRect(r, Paint::Fill(c)); }

void Canvas::strokeRect(Rect r, Color c, f32 width) {
  device_->strokeRect(r, Paint::Stroke(c, width));
}

void Canvas::drawLine(Point p1, Point p2, Color c, f32 width) {
  device_->drawLine(p1, p2, Paint::Stroke(c, width));
}

void Canvas::drawPolyline(const Point *pts, i32 count, Color c, f32 width) {
  device_->drawPolyline(pts, count, Paint::Stroke(c, width));
}

void Canvas::drawText(Point p, std::string_view text, Color c) {
  device_->drawText(p, text, Paint::Fill(c));
}

void Canvas::drawImage(std::shared_ptr<Image> image, f32 x, f32 y) {
  device_->drawImage(std::move(image), x, y);
}

void Canvas::fillCircle(f32 cx, f32 cy, f32 radius, Color c) {
  device_->fillCircle(cx, cy, radius, Paint::Fill(c));
}

void Canvas::strokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width) {
  device_->strokeCircle(cx, cy, radius, Paint::Stroke(c, width));
}

void Canvas::fillRoundRect(Rect r, f32 rx, f32 ry, Color c) {
  device_->fillRoundRect(r, rx, ry, Paint::Fill(c));
}

void Canvas::strokeRoundRect(Rect r, f32 rx, f32 ry, Color c, f32 width) {
  device_->strokeRoundRect(r, rx, ry, Paint::Stroke(c, width));
}

void Canvas::draw(Rect r, const Paint &paint) {
  if (paint.style == PaintStyle::Fill ||
      paint.style == PaintStyle::FillAndStroke) {
    device_->fillRect(r, paint);
  }
  if (paint.style == PaintStyle::Stroke ||
      paint.style == PaintStyle::FillAndStroke) {
    device_->strokeRect(r, paint);
  }
}

void Canvas::draw(Point p1, Point p2, const Paint &paint) {
  device_->drawLine(p1, p2, paint);
}

void Canvas::drawPath(const Path &path, const Paint &paint) {
  device_->drawPath(path, paint);
}

void Canvas::drawImageRect(std::shared_ptr<Image> image, Rect src, Rect dst) {
  Paint p;
  device_->drawImageRect(std::move(image), src, dst, p);
}

void Canvas::drawImageRect(std::shared_ptr<Image> image, Rect dst) {
  if (!image)
    return;
  Rect src = {0, 0, static_cast<f32>(image->width()),
              static_cast<f32>(image->height())};
  drawImageRect(std::move(image), src, dst);
}

// --- State management ---

void Canvas::save() { stack_.push_back(current_); }

void Canvas::restore() {
  if (stack_.empty())
    return;
  current_ = stack_.back();
  stack_.pop_back();
  applyTransform();
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

void Canvas::clipPath(const Path &path) {
  current_.pathClips.push_back(path);
  device_->setClipPath(path);
}

void Canvas::translate(f32 dx, f32 dy) {
  current_.transform *= Matrix::Translate(dx, dy);
  applyTransform();
}

void Canvas::rotate(f32 radians) {
  current_.transform *= Matrix::Rotate(radians);
  applyTransform();
}

void Canvas::scale(f32 sx, f32 sy) {
  current_.transform *= Matrix::Scale(sx, sy);
  applyTransform();
}

void Canvas::concat(const Matrix &m) {
  current_.transform *= m;
  applyTransform();
}

void Canvas::setMatrix(const Matrix &m) {
  current_.transform = m;
  applyTransform();
}

Matrix Canvas::getMatrix() const { return current_.transform; }

void Canvas::applyClip() {
  if (current_.hasClip || !current_.pathClips.empty()) {
    device_->resetClip();
  }
  if (current_.hasClip) {
    device_->setClipRect(current_.clip);
  }
  for (const auto &path : current_.pathClips) {
    device_->setClipPath(path);
  }
  if (!current_.hasClip && current_.pathClips.empty()) {
    device_->resetClip();
  }
}

void Canvas::applyTransform() {
  if (current_.transform.isIdentity()) {
    device_->clearTransform();
  } else {
    device_->setTransform(current_.transform);
  }
}

} // namespace ink
