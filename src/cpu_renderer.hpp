#pragma once

#include "ink/color_filter.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/draw_pass.hpp"
#include "ink/glyph_cache.hpp"
#include "ink/image.hpp"
#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include "ink/path.hpp"
#include "ink/pixmap.hpp"
#include "ink/recording.hpp"
#include "ink/renderer.hpp"
#include "ink/shader.hpp"
#include "path_rasterizer.hpp"
#include "stroke.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ink {

class CpuRenderer : public Renderer, public DrawOpVisitor {
public:
  explicit CpuRenderer(Pixmap *target) : target_(target) {}

  void setGlyphCache(GlyphCache *cache) override { glyphCache_ = cache; }

  void beginFrame(Color clearColor = {0, 0, 0, 255}) override {
    if (target_ && target_->valid()) {
      isBGRA_ = (target_->format() == PixelFormat::BGRA8888);
      target_->clear(clearColor);
    }
    hasClip_ = false;
    clipRect_ = {};
    hasPathClip_ = false;
    clipMask_.clear();
    clipMaskWidth_ = 0;
    currentTransform_ = Matrix::Identity();
  }

  void endFrame() override {}
  void resize(i32, i32) override {}

  void execute(const Recording &recording, const DrawPass &pass) override {
    recording.dispatch(*this, pass);
  }

  std::shared_ptr<Image> makeSnapshot() const override {
    if (target_ && target_->valid()) {
      return Image::MakeFromPixmap(*target_);
    }
    return nullptr;
  }

  // --- Existing visitors (unchanged logic) ---

  void visitFillRect(Rect r, Color c, BlendMode blend, u8 opacity) override {
    c.a = u8(c.a * opacity / 255);
    Rect tr = currentTransform_.isIdentity() ? r : currentTransform_.mapRect(r);
    fillRectScreen(tr, c, blend);
  }

  void visitStrokeRect(Rect r, Color c, f32 width, BlendMode blend,
                       u8 opacity) override {
    c.a = u8(c.a * opacity / 255);
    Rect tr = currentTransform_.isIdentity() ? r : currentTransform_.mapRect(r);
    f32 w = width > 0 ? width : 1;
    if (!currentTransform_.isIdentity() &&
        currentTransform_.isScaleTranslateOnly())
      w *= std::abs(currentTransform_.scaleX);
    i32 iw = i32(w);
    fillRectScreen({tr.x, tr.y, tr.w, f32(iw)}, c, blend);
    fillRectScreen({tr.x, tr.y + tr.h - iw, tr.w, f32(iw)}, c, blend);
    fillRectScreen({tr.x, tr.y + iw, f32(iw), tr.h - iw * 2}, c, blend);
    fillRectScreen({tr.x + tr.w - iw, tr.y + iw, f32(iw), tr.h - iw * 2}, c,
                   blend);
  }

  void visitLine(Point p1, Point p2, Color c, f32 width, BlendMode blend,
                 u8 opacity) override {
    c.a = u8(c.a * opacity / 255);
    Point tp1 = currentTransform_.mapPoint(p1);
    Point tp2 = currentTransform_.mapPoint(p2);
    i32 w = i32(width > 0 ? width : 1);
    i32 x0 = i32(tp1.x), y0 = i32(tp1.y);
    i32 x1 = i32(tp2.x), y1 = i32(tp2.y);
    i32 dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    i32 sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    i32 err = dx - dy;
    while (true) {
      for (i32 oy = -w / 2; oy <= w / 2; ++oy)
        for (i32 ox = -w / 2; ox <= w / 2; ++ox)
          blendPixel(x0 + ox, y0 + oy, c, blend);
      if (x0 == x1 && y0 == y1)
        break;
      i32 e2 = 2 * err;
      if (e2 > -dy) {
        err -= dy;
        x0 += sx;
      }
      if (e2 < dx) {
        err += dx;
        y0 += sy;
      }
    }
  }

  void visitPolyline(const Point *pts, i32 count, Color c, f32 width,
                     BlendMode blend, u8 opacity) override {
    for (i32 i = 0; i + 1 < count; ++i)
      visitLine(pts[i], pts[i + 1], c, width, blend, opacity);
  }

  void visitText(Point p, const char *text, u32 len, Color c, BlendMode,
                 u8) override {
    if (!glyphCache_ || !target_)
      return;
    Point tp = currentTransform_.mapPoint(p);
    u32 *pixels = static_cast<u32 *>(target_->addr());
    i32 strideU32 = target_->stride() / target_->info().bytesPerPixel();
    glyphCache_->drawText(pixels, strideU32, target_->height(), i32(tp.x),
                          i32(tp.y), std::string_view(text, len), c);
  }

  void visitDrawImage(const Image *image, f32 x, f32 y, BlendMode,
                      u8) override {
    if (!image || !image->valid() || !target_)
      return;
    if (!image->isCpuBacked())
      return;
    Point tp = currentTransform_.mapPoint({x, y});
    i32 ix = i32(tp.x), iy = i32(tp.y);
    drawImagePixels(image, ix, iy,
                    {0, 0, f32(image->width()), f32(image->height())});
  }

  void visitSetClip(Rect r) override {
    clipRect_ = r;
    hasClip_ = true;
  }

  void visitSetClipPath(const Path &path, u8 fillRule,
                        bool antiAlias) override {
    if (!target_ || !target_->valid())
      return;

    Pixmap mask = Pixmap::Alloc(
        PixmapInfo::MakeRGBA(target_->width(), target_->height()));
    if (!mask.valid())
      return;
    mask.clear({0, 0, 0, 0});

    PathRasterizer clipRasterizer;
    clipRasterizer.addPath(path, currentTransform_);
    FillRule fr = fillRule ? FillRule::EvenOdd : FillRule::Winding;
    clipRasterizer.rasterize(&mask, effectiveClip(), {255, 255, 255, 255},
                             BlendMode::Src, 255, antiAlias, fr, nullptr,
                             false);

    i32 count = target_->width() * target_->height();
    if (!hasPathClip_ || clipMaskWidth_ != target_->width() ||
        static_cast<i32>(clipMask_.size()) != count) {
      clipMask_.assign(count, 255);
      clipMaskWidth_ = target_->width();
    }

    const u32 *pixels = mask.addr32();
    for (i32 i = 0; i < count; ++i) {
      u8 alpha = static_cast<u8>((pixels[i] >> 24) & 0xFF);
      clipMask_[i] = static_cast<u8>(clipMask_[i] * alpha / 255);
    }
    hasPathClip_ = true;
  }

  void visitClearClip() override {
    hasClip_ = false;
    hasPathClip_ = false;
    clipMask_.clear();
    clipMaskWidth_ = 0;
  }
  void visitSetTransform(const Matrix &m) override { currentTransform_ = m; }
  void visitClearTransform() override {
    currentTransform_ = Matrix::Identity();
  }

  void visitFillCircle(f32 cx, f32 cy, f32 radius, Color c, BlendMode,
                       u8 opacity) override {
    c.a = u8(c.a * opacity / 255);
    Point center = currentTransform_.mapPoint({cx, cy});
    f32 r = radius;
    if (!currentTransform_.isIdentity() &&
        currentTransform_.isScaleTranslateOnly())
      r *= std::abs(currentTransform_.scaleX);
    Rect bounds = {center.x - r, center.y - r, r * 2, r * 2};
    Rect clip = effectiveClip();
    i32 x0 = std::max(i32(bounds.x), i32(clip.x));
    i32 y0 = std::max(i32(bounds.y), i32(clip.y));
    i32 x1 = std::min(i32(bounds.x + bounds.w + 1), i32(clip.x + clip.w));
    i32 y1 = std::min(i32(bounds.y + bounds.h + 1), i32(clip.y + clip.h));
    f32 r2 = r * r;
    for (i32 y = y0; y < y1; ++y)
      for (i32 x = x0; x < x1; ++x) {
        f32 dx = (x + 0.5f) - center.x, dy = (y + 0.5f) - center.y;
        if (dx * dx + dy * dy <= r2)
          blendPixel(x, y, c);
      }
  }

  void visitStrokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width,
                         BlendMode, u8 opacity) override {
    c.a = u8(c.a * opacity / 255);
    Point center = currentTransform_.mapPoint({cx, cy});
    f32 r = radius, w = width > 0 ? width : 1.0f;
    if (!currentTransform_.isIdentity() &&
        currentTransform_.isScaleTranslateOnly()) {
      f32 s = std::abs(currentTransform_.scaleX);
      r *= s;
      w *= s;
    }
    f32 hw = w * 0.5f, outerR = r + hw, innerR = std::max(0.0f, r - hw);
    Rect bounds = {center.x - outerR, center.y - outerR, outerR * 2,
                   outerR * 2};
    Rect clip = effectiveClip();
    i32 x0 = std::max(i32(bounds.x), i32(clip.x));
    i32 y0 = std::max(i32(bounds.y), i32(clip.y));
    i32 x1 = std::min(i32(bounds.x + bounds.w + 1), i32(clip.x + clip.w));
    i32 y1 = std::min(i32(bounds.y + bounds.h + 1), i32(clip.y + clip.h));
    f32 outerR2 = outerR * outerR, innerR2 = innerR * innerR;
    for (i32 y = y0; y < y1; ++y)
      for (i32 x = x0; x < x1; ++x) {
        f32 dx = (x + 0.5f) - center.x, dy = (y + 0.5f) - center.y;
        f32 dist2 = dx * dx + dy * dy;
        if (dist2 <= outerR2 && dist2 >= innerR2)
          blendPixel(x, y, c);
      }
  }

  void visitFillRoundRect(Rect rect, f32 rx, f32 ry, Color c, BlendMode,
                          u8 opacity) override {
    c.a = u8(c.a * opacity / 255);
    Rect tr =
        currentTransform_.isIdentity() ? rect : currentTransform_.mapRect(rect);
    f32 crx = rx, cry = ry;
    if (!currentTransform_.isIdentity() &&
        currentTransform_.isScaleTranslateOnly()) {
      crx *= std::abs(currentTransform_.scaleX);
      cry *= std::abs(currentTransform_.scaleY);
    }
    crx = std::min(crx, tr.w * 0.5f);
    cry = std::min(cry, tr.h * 0.5f);
    Rect clip = effectiveClip();
    i32 x0 = std::max(i32(tr.x), i32(clip.x));
    i32 y0 = std::max(i32(tr.y), i32(clip.y));
    i32 x1 = std::min(i32(tr.x + tr.w), i32(clip.x + clip.w));
    i32 y1 = std::min(i32(tr.y + tr.h), i32(clip.y + clip.h));
    for (i32 y = y0; y < y1; ++y)
      for (i32 x = x0; x < x1; ++x)
        if (isInsideRoundRect(f32(x) + 0.5f, f32(y) + 0.5f, tr, crx, cry))
          blendPixel(x, y, c);
  }

  void visitStrokeRoundRect(Rect rect, f32 rx, f32 ry, Color c, f32 width,
                            BlendMode, u8 opacity) override {
    c.a = u8(c.a * opacity / 255);
    Rect tr =
        currentTransform_.isIdentity() ? rect : currentTransform_.mapRect(rect);
    f32 w = width > 0 ? width : 1.0f, crx = rx, cry = ry;
    if (!currentTransform_.isIdentity() &&
        currentTransform_.isScaleTranslateOnly()) {
      f32 sx = std::abs(currentTransform_.scaleX),
          sy = std::abs(currentTransform_.scaleY);
      crx *= sx;
      cry *= sy;
      w *= sx;
    }
    crx = std::min(crx, tr.w * 0.5f);
    cry = std::min(cry, tr.h * 0.5f);
    f32 hw = w * 0.5f;
    Rect outer = {tr.x - hw, tr.y - hw, tr.w + w, tr.h + w};
    Rect inner = {tr.x + hw, tr.y + hw, std::max(0.0f, tr.w - w),
                  std::max(0.0f, tr.h - w)};
    f32 outerRx = crx + hw, outerRy = cry + hw;
    f32 innerRx = std::max(0.0f, crx - hw), innerRy = std::max(0.0f, cry - hw);
    Rect clip = effectiveClip();
    i32 x0 = std::max(i32(outer.x), i32(clip.x));
    i32 y0 = std::max(i32(outer.y), i32(clip.y));
    i32 x1 = std::min(i32(outer.x + outer.w + 1), i32(clip.x + clip.w));
    i32 y1 = std::min(i32(outer.y + outer.h + 1), i32(clip.y + clip.h));
    for (i32 y = y0; y < y1; ++y)
      for (i32 x = x0; x < x1; ++x) {
        f32 px = f32(x) + 0.5f, py = f32(y) + 0.5f;
        bool inOuter = isInsideRoundRect(px, py, outer, outerRx, outerRy);
        bool inInner = (inner.w > 0 && inner.h > 0) &&
                       isInsideRoundRect(px, py, inner, innerRx, innerRy);
        if (inOuter && !inInner)
          blendPixel(x, y, c);
      }
  }

  // --- New Phase 1 visitors ---

  void visitFillPath(const Path &path, Color color, BlendMode blend, u8 opacity,
                     u8 fillRule, bool antiAlias, const Shader *shader,
                     const ColorFilter *) override {
    if (!target_ || !target_->valid())
      return;
    rasterizer_.reset();
    rasterizer_.addPath(path, currentTransform_);
    FillRule fr = fillRule ? FillRule::EvenOdd : FillRule::Winding;
    rasterizer_.rasterize(
        target_, effectiveClip(), color, blend, opacity, antiAlias, fr, shader,
        isBGRA_, hasPathClip_ ? clipMask_.data() : nullptr, clipMaskWidth_);
  }

  void visitStrokePath(const Path &path, Color color, f32 width,
                       BlendMode blend, u8 opacity, u8 cap, u8 join,
                       f32 miterLimit, bool antiAlias, const Shader *shader,
                       const ColorFilter *) override {
    if (!target_ || !target_->valid())
      return;
    Path stroked =
        StrokeEngine::strokePath(path, width, static_cast<LineCap>(cap),
                                 static_cast<LineJoin>(join), miterLimit);
    rasterizer_.reset();
    rasterizer_.addPath(stroked, currentTransform_);
    rasterizer_.rasterize(target_, effectiveClip(), color, blend, opacity,
                          antiAlias, FillRule::Winding, shader, isBGRA_,
                          hasPathClip_ ? clipMask_.data() : nullptr,
                          clipMaskWidth_);
  }

  void visitDrawImageRect(const Image *image, Rect src, Rect dst, BlendMode, u8,
                          FilterQuality) override {
    if (!image || !image->valid() || !target_)
      return;
    if (!image->isCpuBacked())
      return;
    Point tp = currentTransform_.mapPoint({dst.x, dst.y});
    // Simple nearest-neighbor scaling
    drawImageScaled(image, src, {tp.x, tp.y, dst.w, dst.h});
  }

private:
  Pixmap *target_ = nullptr;
  GlyphCache *glyphCache_ = nullptr;
  Rect clipRect_ = {};
  bool hasClip_ = false;
  bool hasPathClip_ = false;
  bool isBGRA_ = true;
  Matrix currentTransform_;
  PathRasterizer rasterizer_;
  std::vector<u8> clipMask_;
  i32 clipMaskWidth_ = 0;

  static bool isInsideRoundRect(f32 px, f32 py, Rect r, f32 rx, f32 ry) {
    if (px < r.x || px > r.x + r.w || py < r.y || py > r.y + r.h)
      return false;
    f32 left = r.x + rx, right = r.x + r.w - rx;
    f32 top = r.y + ry, bottom = r.y + r.h - ry;
    f32 dx = 0, dy = 0;
    if (px < left && py < top) {
      dx = (px - left) / rx;
      dy = (py - top) / ry;
    } else if (px > right && py < top) {
      dx = (px - right) / rx;
      dy = (py - top) / ry;
    } else if (px < left && py > bottom) {
      dx = (px - left) / rx;
      dy = (py - bottom) / ry;
    } else if (px > right && py > bottom) {
      dx = (px - right) / rx;
      dy = (py - bottom) / ry;
    } else
      return true;
    return (dx * dx + dy * dy) <= 1.0f;
  }

  bool isClipped(i32 x, i32 y) const {
    if (hasClip_ && (x < clipRect_.x || x >= clipRect_.x + clipRect_.w ||
                     y < clipRect_.y || y >= clipRect_.y + clipRect_.h))
      return true;
    if (hasPathClip_ && clipMaskWidth_ > 0) {
      return clipMask_[y * clipMaskWidth_ + x] == 0;
    }
    return false;
  }

  void fillRectScreen(Rect r, Color c, BlendMode mode = BlendMode::SrcOver) {
    Rect clip = effectiveClip();
    i32 x0 = std::max(i32(r.x), i32(clip.x));
    i32 y0 = std::max(i32(r.y), i32(clip.y));
    i32 x1 = std::min(i32(r.x + r.w), i32(clip.x + clip.w));
    i32 y1 = std::min(i32(r.y + r.h), i32(clip.y + clip.h));
    for (i32 y = y0; y < y1; ++y)
      for (i32 x = x0; x < x1; ++x)
        blendPixel(x, y, c, mode);
  }

  Rect effectiveClip() const {
    if (!hasClip_)
      return {0, 0, f32(target_->width()), f32(target_->height())};
    return clipRect_;
  }

  Color decodePixel(u32 raw) const {
    if (isBGRA_)
      return {u8((raw >> 16) & 0xFF), u8((raw >> 8) & 0xFF), u8(raw & 0xFF),
              u8((raw >> 24) & 0xFF)};
    return {u8(raw & 0xFF), u8((raw >> 8) & 0xFF), u8((raw >> 16) & 0xFF),
            u8((raw >> 24) & 0xFF)};
  }

  u32 encodePixel(Color c) const {
    if (isBGRA_)
      return (u32(c.a) << 24) | (u32(c.r) << 16) | (u32(c.g) << 8) | u32(c.b);
    return (u32(c.a) << 24) | (u32(c.b) << 16) | (u32(c.g) << 8) | u32(c.r);
  }

  Color blendColors(Color src, Color dst, BlendMode mode) const {
    u32 sa = src.a, da = dst.a;
    u32 invSa = 255 - sa, invDa = 255 - da;
    u32 fa = 0, fb = 0;
    switch (mode) {
    case BlendMode::SrcOver:
      fa = 255;
      fb = invSa;
      break;
    case BlendMode::Src:
      fa = 255;
      fb = 0;
      break;
    case BlendMode::Dst:
      fa = 0;
      fb = 255;
      break;
    case BlendMode::SrcIn:
      fa = da;
      fb = 0;
      break;
    case BlendMode::DstIn:
      fa = 0;
      fb = sa;
      break;
    case BlendMode::SrcOut:
      fa = invDa;
      fb = 0;
      break;
    case BlendMode::DstOut:
      fa = 0;
      fb = invSa;
      break;
    case BlendMode::SrcAtop:
      fa = da;
      fb = invSa;
      break;
    case BlendMode::DstAtop:
      fa = invDa;
      fb = sa;
      break;
    case BlendMode::Xor:
      fa = invDa;
      fb = invSa;
      break;
    case BlendMode::Clear:
      return {0, 0, 0, 0};
    }
    return {u8((src.r * sa * fa / 255 + dst.r * da * fb / 255) / 255),
            u8((src.g * sa * fa / 255 + dst.g * da * fb / 255) / 255),
            u8((src.b * sa * fa / 255 + dst.b * da * fb / 255) / 255),
            u8((sa * fa + da * fb) / 255)};
  }

  void blendPixel(i32 x, i32 y, Color c, BlendMode mode = BlendMode::SrcOver) {
    if (!target_ || !target_->valid())
      return;
    if (x < 0 || x >= target_->width() || y < 0 || y >= target_->height())
      return;
    if (isClipped(x, y))
      return;
    if (hasPathClip_ && clipMaskWidth_ > 0) {
      u8 maskAlpha = clipMask_[y * clipMaskWidth_ + x];
      c.a = static_cast<u8>(c.a * maskAlpha / 255);
      if (c.a == 0)
        return;
    }
    u32 *row = static_cast<u32 *>(target_->rowAddr(y));
    u32 &pixel = row[x];
    Color dst = decodePixel(pixel);
    pixel = encodePixel(blendColors(c, dst, mode));
  }

  void drawImagePixels(const Image *image, i32 ix, i32 iy, Rect src) {
    i32 srcX0 = i32(src.x), srcY0 = i32(src.y);
    i32 srcW = i32(src.w), srcH = i32(src.h);
    const u8 *srcPixels = static_cast<const u8 *>(image->pixels());
    i32 srcStride = image->stride();
    bool srcBGRA = (image->format() == PixelFormat::BGRA8888);
    for (i32 sy = 0; sy < srcH; ++sy) {
      const u8 *srcRow = srcPixels + (srcY0 + sy) * srcStride;
      for (i32 sx = 0; sx < srcW; ++sx) {
        i32 dx = ix + sx, dy = iy + sy;
        if (dx < 0 || dx >= target_->width() || dy < 0 ||
            dy >= target_->height())
          continue;
        if (isClipped(dx, dy))
          continue;
        const u8 *s = srcRow + (srcX0 + sx) * 4;
        Color c = srcBGRA ? Color{s[2], s[1], s[0], s[3]}
                          : Color{s[0], s[1], s[2], s[3]};
        blendPixel(dx, dy, c);
      }
    }
  }

  void drawImageScaled(const Image *image, Rect src, Rect dst) {
    if (dst.w <= 0 || dst.h <= 0 || src.w <= 0 || src.h <= 0)
      return;
    const u8 *srcPixels = static_cast<const u8 *>(image->pixels());
    i32 srcStride = image->stride();
    bool srcBGRA = (image->format() == PixelFormat::BGRA8888);
    Rect clip = effectiveClip();
    i32 dx0 = std::max(i32(dst.x), i32(clip.x));
    i32 dy0 = std::max(i32(dst.y), i32(clip.y));
    i32 dx1 = std::min(i32(dst.x + dst.w), i32(clip.x + clip.w));
    i32 dy1 = std::min(i32(dst.y + dst.h), i32(clip.y + clip.h));
    for (i32 dy = dy0; dy < dy1; ++dy) {
      f32 sv = src.y + (dy - dst.y) / dst.h * src.h;
      i32 sy = std::min(i32(sv), i32(src.y + src.h - 1));
      const u8 *srcRow = srcPixels + sy * srcStride;
      for (i32 dx = dx0; dx < dx1; ++dx) {
        f32 su = src.x + (dx - dst.x) / dst.w * src.w;
        i32 sx = std::min(i32(su), i32(src.x + src.w - 1));
        const u8 *s = srcRow + sx * 4;
        Color c = srcBGRA ? Color{s[2], s[1], s[0], s[3]}
                          : Color{s[0], s[1], s[2], s[3]};
        blendPixel(dx, dy, c);
      }
    }
  }
};

} // namespace ink
