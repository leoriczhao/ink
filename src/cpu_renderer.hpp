#pragma once

#include "ink/renderer.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/pixmap.hpp"
#include "ink/recording.hpp"
#include "ink/draw_pass.hpp"
#include "ink/image.hpp"
#include "ink/glyph_cache.hpp"
#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include <cmath>

namespace ink {

class CpuRenderer : public Renderer, public DrawOpVisitor {
public:
    explicit CpuRenderer(Pixmap* target) : target_(target) {}

    void setGlyphCache(GlyphCache* cache) override { glyphCache_ = cache; }

    // Renderer interface
    void beginFrame(Color clearColor = {0, 0, 0, 255}) override {
        if (target_ && target_->valid()) {
            isBGRA_ = (target_->format() == PixelFormat::BGRA8888);
            target_->clear(clearColor);
        }
        hasClip_ = false;
        clipRect_ = {};
        currentTransform_ = Matrix::Identity();
    }

    void endFrame() override {}

    void resize(i32, i32) override {}

    void execute(const Recording& recording, const DrawPass& pass) override {
        recording.dispatch(*this, pass);
    }

    std::shared_ptr<Image> makeSnapshot() const override {
        if (target_ && target_->valid()) {
            return Image::MakeFromPixmap(*target_);
        }
        return nullptr;
    }

    // DrawOpVisitor interface
    void visitFillRect(Rect r, Color c, BlendMode blend, u8 opacity) override {
        c.a = u8(c.a * opacity / 255);
        Rect tr = currentTransform_.isIdentity() ? r : currentTransform_.mapRect(r);
        fillRectScreen(tr, c, blend);
    }

    void visitStrokeRect(Rect r, Color c, f32 width, BlendMode blend, u8 opacity) override {
        c.a = u8(c.a * opacity / 255);
        Rect tr = currentTransform_.isIdentity() ? r : currentTransform_.mapRect(r);
        f32 w = width > 0 ? width : 1;
        if (!currentTransform_.isIdentity() && currentTransform_.isScaleTranslateOnly()) {
            w *= std::abs(currentTransform_.scaleX);
        }
        i32 iw = i32(w);
        fillRectScreen({tr.x, tr.y, tr.w, f32(iw)}, c, blend);
        fillRectScreen({tr.x, tr.y + tr.h - iw, tr.w, f32(iw)}, c, blend);
        fillRectScreen({tr.x, tr.y + iw, f32(iw), tr.h - iw * 2}, c, blend);
        fillRectScreen({tr.x + tr.w - iw, tr.y + iw, f32(iw), tr.h - iw * 2}, c, blend);
    }

    void visitLine(Point p1, Point p2, Color c, f32 width, BlendMode blend, u8 opacity) override {
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
            for (i32 oy = -w/2; oy <= w/2; ++oy) {
                for (i32 ox = -w/2; ox <= w/2; ++ox) {
                    blendPixel(x0 + ox, y0 + oy, c, blend);
                }
            }
            if (x0 == x1 && y0 == y1) break;
            i32 e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }
    }

    void visitPolyline(const Point* pts, i32 count, Color c, f32 width, BlendMode blend, u8 opacity) override {
        for (i32 i = 0; i + 1 < count; ++i) {
            visitLine(pts[i], pts[i + 1], c, width, blend, opacity);
        }
    }

    void visitText(Point p, const char* text, u32 len, Color c, BlendMode, u8) override {
        if (!glyphCache_ || !target_) return;
        Point tp = currentTransform_.mapPoint(p);
        u32* pixels = static_cast<u32*>(target_->addr());
        // Stride in u32 units (not bytes, not pixel width)
        i32 strideU32 = target_->stride() / target_->info().bytesPerPixel();
        glyphCache_->drawText(pixels, strideU32, target_->height(),
                              i32(tp.x), i32(tp.y), std::string_view(text, len), c);
    }

    void visitDrawImage(const Image* image, f32 x, f32 y, BlendMode, u8) override {
        if (!image || !image->valid() || !target_) return;
        if (!image->isCpuBacked()) return;

        Point tp = currentTransform_.mapPoint({x, y});
        i32 ix = i32(tp.x), iy = i32(tp.y);
        i32 iw = image->width(), ih = image->height();
        const u8* srcPixels = static_cast<const u8*>(image->pixels());
        i32 srcStride = image->stride();
        bool srcBGRA = (image->format() == PixelFormat::BGRA8888);

        for (i32 sy = 0; sy < ih; ++sy) {
            const u8* srcRow = srcPixels + sy * srcStride;
            for (i32 sx = 0; sx < iw; ++sx) {
                i32 dx = ix + sx, dy = iy + sy;
                if (dx < 0 || dx >= target_->width() || dy < 0 || dy >= target_->height()) continue;
                if (isClipped(dx, dy)) continue;

                const u8* src = srcRow + sx * 4;
                Color c;
                if (srcBGRA) {
                    c = {src[2], src[1], src[0], src[3]};
                } else {
                    c = {src[0], src[1], src[2], src[3]};
                }
                blendPixel(dx, dy, c);
            }
        }
    }

    void visitSetClip(Rect r) override {
        clipRect_ = r;
        hasClip_ = true;
    }

    void visitClearClip() override {
        hasClip_ = false;
    }

    void visitSetTransform(const Matrix& m) override {
        currentTransform_ = m;
    }

    void visitClearTransform() override {
        currentTransform_ = Matrix::Identity();
    }

    void visitFillCircle(f32 cx, f32 cy, f32 radius, Color c, BlendMode, u8 opacity) override {
        c.a = u8(c.a * opacity / 255);
        Point center = currentTransform_.mapPoint({cx, cy});
        f32 r = radius;
        if (!currentTransform_.isIdentity() && currentTransform_.isScaleTranslateOnly()) {
            r *= std::abs(currentTransform_.scaleX);
        }
        Rect bounds = {center.x - r, center.y - r, r * 2, r * 2};
        Rect clip = effectiveClip();
        i32 x0 = std::max(i32(bounds.x), i32(clip.x));
        i32 y0 = std::max(i32(bounds.y), i32(clip.y));
        i32 x1 = std::min(i32(bounds.x + bounds.w + 1), i32(clip.x + clip.w));
        i32 y1 = std::min(i32(bounds.y + bounds.h + 1), i32(clip.y + clip.h));
        f32 r2 = r * r;
        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                f32 dx = (x + 0.5f) - center.x;
                f32 dy = (y + 0.5f) - center.y;
                if (dx * dx + dy * dy <= r2) {
                    blendPixel(x, y, c);
                }
            }
        }
    }

    void visitStrokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width, BlendMode, u8 opacity) override {
        c.a = u8(c.a * opacity / 255);
        Point center = currentTransform_.mapPoint({cx, cy});
        f32 r = radius;
        f32 w = width > 0 ? width : 1.0f;
        if (!currentTransform_.isIdentity() && currentTransform_.isScaleTranslateOnly()) {
            f32 s = std::abs(currentTransform_.scaleX);
            r *= s;
            w *= s;
        }
        f32 hw = w * 0.5f;
        f32 outerR = r + hw;
        f32 innerR = r - hw;
        if (innerR < 0) innerR = 0;
        Rect bounds = {center.x - outerR, center.y - outerR, outerR * 2, outerR * 2};
        Rect clip = effectiveClip();
        i32 x0 = std::max(i32(bounds.x), i32(clip.x));
        i32 y0 = std::max(i32(bounds.y), i32(clip.y));
        i32 x1 = std::min(i32(bounds.x + bounds.w + 1), i32(clip.x + clip.w));
        i32 y1 = std::min(i32(bounds.y + bounds.h + 1), i32(clip.y + clip.h));
        f32 outerR2 = outerR * outerR;
        f32 innerR2 = innerR * innerR;
        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                f32 dx = (x + 0.5f) - center.x;
                f32 dy = (y + 0.5f) - center.y;
                f32 dist2 = dx * dx + dy * dy;
                if (dist2 <= outerR2 && dist2 >= innerR2) {
                    blendPixel(x, y, c);
                }
            }
        }
    }

    void visitFillRoundRect(Rect rect, f32 rx, f32 ry, Color c, BlendMode, u8 opacity) override {
        c.a = u8(c.a * opacity / 255);
        Rect tr = currentTransform_.isIdentity() ? rect : currentTransform_.mapRect(rect);
        f32 crx = rx, cry = ry;
        if (!currentTransform_.isIdentity() && currentTransform_.isScaleTranslateOnly()) {
            crx *= std::abs(currentTransform_.scaleX);
            cry *= std::abs(currentTransform_.scaleY);
        }
        // Clamp corner radii to half the rect dimensions
        crx = std::min(crx, tr.w * 0.5f);
        cry = std::min(cry, tr.h * 0.5f);

        Rect clip = effectiveClip();
        i32 x0 = std::max(i32(tr.x), i32(clip.x));
        i32 y0 = std::max(i32(tr.y), i32(clip.y));
        i32 x1 = std::min(i32(tr.x + tr.w), i32(clip.x + clip.w));
        i32 y1 = std::min(i32(tr.y + tr.h), i32(clip.y + clip.h));

        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                if (isInsideRoundRect(f32(x) + 0.5f, f32(y) + 0.5f, tr, crx, cry)) {
                    blendPixel(x, y, c);
                }
            }
        }
    }

    void visitStrokeRoundRect(Rect rect, f32 rx, f32 ry, Color c, f32 width, BlendMode, u8 opacity) override {
        c.a = u8(c.a * opacity / 255);
        Rect tr = currentTransform_.isIdentity() ? rect : currentTransform_.mapRect(rect);
        f32 w = width > 0 ? width : 1.0f;
        f32 crx = rx, cry = ry;
        if (!currentTransform_.isIdentity() && currentTransform_.isScaleTranslateOnly()) {
            f32 sx = std::abs(currentTransform_.scaleX);
            f32 sy = std::abs(currentTransform_.scaleY);
            crx *= sx;
            cry *= sy;
            w *= sx;
        }
        crx = std::min(crx, tr.w * 0.5f);
        cry = std::min(cry, tr.h * 0.5f);

        f32 hw = w * 0.5f;
        // Outer rect expanded by half-width, inner rect shrunk by half-width
        Rect outer = {tr.x - hw, tr.y - hw, tr.w + w, tr.h + w};
        Rect inner = {tr.x + hw, tr.y + hw, tr.w - w, tr.h - w};
        f32 outerRx = crx + hw, outerRy = cry + hw;
        f32 innerRx = crx - hw, innerRy = cry - hw;
        if (innerRx < 0) innerRx = 0;
        if (innerRy < 0) innerRy = 0;
        if (inner.w < 0) inner.w = 0;
        if (inner.h < 0) inner.h = 0;

        Rect clip = effectiveClip();
        i32 x0 = std::max(i32(outer.x), i32(clip.x));
        i32 y0 = std::max(i32(outer.y), i32(clip.y));
        i32 x1 = std::min(i32(outer.x + outer.w + 1), i32(clip.x + clip.w));
        i32 y1 = std::min(i32(outer.y + outer.h + 1), i32(clip.y + clip.h));

        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                f32 px = f32(x) + 0.5f;
                f32 py = f32(y) + 0.5f;
                bool inOuter = isInsideRoundRect(px, py, outer, outerRx, outerRy);
                bool inInner = (inner.w > 0 && inner.h > 0) &&
                               isInsideRoundRect(px, py, inner, innerRx, innerRy);
                if (inOuter && !inInner) {
                    blendPixel(x, y, c);
                }
            }
        }
    }

private:
    Pixmap* target_ = nullptr;
    GlyphCache* glyphCache_ = nullptr;
    Rect clipRect_ = {};
    bool hasClip_ = false;
    bool isBGRA_ = true;
    Matrix currentTransform_;

    /// @brief Test if a point is inside a rounded rectangle.
    static bool isInsideRoundRect(f32 px, f32 py, Rect r, f32 rx, f32 ry) {
        // First check if inside the bounding rect at all
        if (px < r.x || px > r.x + r.w || py < r.y || py > r.y + r.h)
            return false;

        // Check corners: if we're in a corner region, test ellipse distance
        f32 left = r.x + rx;
        f32 right = r.x + r.w - rx;
        f32 top = r.y + ry;
        f32 bottom = r.y + r.h - ry;

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
        } else {
            return true; // In the cross region, not a corner
        }
        return (dx * dx + dy * dy) <= 1.0f;
    }

    bool isClipped(i32 x, i32 y) const {
        if (!hasClip_) return false;
        return x < clipRect_.x || x >= clipRect_.x + clipRect_.w ||
               y < clipRect_.y || y >= clipRect_.y + clipRect_.h;
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
        if (!hasClip_) return {0, 0, f32(target_->width()), f32(target_->height())};
        return clipRect_;
    }

    Color decodePixel(u32 raw) const {
        if (isBGRA_) {
            return {u8((raw >> 16) & 0xFF), u8((raw >> 8) & 0xFF),
                    u8(raw & 0xFF), u8((raw >> 24) & 0xFF)};
        }
        return {u8(raw & 0xFF), u8((raw >> 8) & 0xFF),
                u8((raw >> 16) & 0xFF), u8((raw >> 24) & 0xFF)};
    }

    u32 encodePixel(Color c) const {
        if (isBGRA_) {
            return (u32(c.a) << 24) | (u32(c.r) << 16) | (u32(c.g) << 8) | u32(c.b);
        }
        return (u32(c.a) << 24) | (u32(c.b) << 16) | (u32(c.g) << 8) | u32(c.r);
    }

    Color blendColors(Color src, Color dst, BlendMode mode) const {
        u32 sa = src.a, da = dst.a;
        u32 invSa = 255 - sa, invDa = 255 - da;
        // Porter-Duff: Out = Fa*Src + Fb*Dst (alpha-premultiplied)
        // Fa and Fb operate on premultiplied colors: src.c*src.a and dst.c*dst.a
        u32 fa = 0, fb = 0;
        switch (mode) {
            case BlendMode::SrcOver: fa = 255;  fb = invSa; break;
            case BlendMode::Src:     fa = 255;  fb = 0;     break;
            case BlendMode::Dst:     fa = 0;    fb = 255;   break;
            case BlendMode::SrcIn:   fa = da;   fb = 0;     break;
            case BlendMode::DstIn:   fa = 0;    fb = sa;    break;
            case BlendMode::SrcOut:  fa = invDa; fb = 0;    break;
            case BlendMode::DstOut:  fa = 0;    fb = invSa; break;
            case BlendMode::SrcAtop: fa = da;   fb = invSa; break;
            case BlendMode::DstAtop: fa = invDa; fb = sa;   break;
            case BlendMode::Xor:     fa = invDa; fb = invSa; break;
            case BlendMode::Clear:   return {0, 0, 0, 0};
        }
        // Apply as premultiplied: src color is weighted by src.a, dst by dst.a
        u8 outR = u8((src.r * sa * fa / 255 + dst.r * da * fb / 255) / 255);
        u8 outG = u8((src.g * sa * fa / 255 + dst.g * da * fb / 255) / 255);
        u8 outB = u8((src.b * sa * fa / 255 + dst.b * da * fb / 255) / 255);
        u8 outA = u8((sa * fa + da * fb) / 255);
        return {outR, outG, outB, outA};
    }

    void blendPixel(i32 x, i32 y, Color c, BlendMode mode = BlendMode::SrcOver) {
        if (!target_ || !target_->valid()) return;
        if (x < 0 || x >= target_->width() || y < 0 || y >= target_->height()) return;
        if (isClipped(x, y)) return;

        u32* row = static_cast<u32*>(target_->rowAddr(y));
        u32& pixel = row[x];

        Color dst = decodePixel(pixel);
        Color out = blendColors(c, dst, mode);
        pixel = encodePixel(out);
    }
};

} // namespace ink
