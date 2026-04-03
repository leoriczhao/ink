#pragma once

#include "ink/renderer.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/pixmap.hpp"
#include "ink/recording.hpp"
#include "ink/draw_pass.hpp"
#include "ink/image.hpp"
#include "ink/glyph_cache.hpp"
#include "ink/matrix.hpp"
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
    void visitFillRect(Rect r, Color c) override {
        Rect tr = currentTransform_.isIdentity() ? r : currentTransform_.mapRect(r);
        fillRectScreen(tr, c);
    }

    void visitStrokeRect(Rect r, Color c, f32 width) override {
        Rect tr = currentTransform_.isIdentity() ? r : currentTransform_.mapRect(r);
        f32 w = width > 0 ? width : 1;
        if (!currentTransform_.isIdentity() && currentTransform_.isScaleTranslateOnly()) {
            w *= std::abs(currentTransform_.scaleX);
        }
        i32 iw = i32(w);
        fillRectScreen({tr.x, tr.y, tr.w, f32(iw)}, c);
        fillRectScreen({tr.x, tr.y + tr.h - iw, tr.w, f32(iw)}, c);
        fillRectScreen({tr.x, tr.y + iw, f32(iw), tr.h - iw * 2}, c);
        fillRectScreen({tr.x + tr.w - iw, tr.y + iw, f32(iw), tr.h - iw * 2}, c);
    }

    void visitLine(Point p1, Point p2, Color c, f32 width) override {
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
                    blendPixel(x0 + ox, y0 + oy, c);
                }
            }
            if (x0 == x1 && y0 == y1) break;
            i32 e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }
    }

    void visitPolyline(const Point* pts, i32 count, Color c, f32 width) override {
        for (i32 i = 0; i + 1 < count; ++i) {
            visitLine(pts[i], pts[i + 1], c, width);
        }
    }

    void visitText(Point p, const char* text, u32 len, Color c) override {
        if (!glyphCache_ || !target_) return;
        Point tp = currentTransform_.mapPoint(p);
        u32* pixels = static_cast<u32*>(target_->addr());
        // Stride in u32 units (not bytes, not pixel width)
        i32 strideU32 = target_->stride() / target_->info().bytesPerPixel();
        glyphCache_->drawText(pixels, strideU32, target_->height(),
                              i32(tp.x), i32(tp.y), std::string_view(text, len), c);
    }

    void visitDrawImage(const Image* image, f32 x, f32 y) override {
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

private:
    Pixmap* target_ = nullptr;
    GlyphCache* glyphCache_ = nullptr;
    Rect clipRect_ = {};
    bool hasClip_ = false;
    bool isBGRA_ = true;
    Matrix currentTransform_;

    bool isClipped(i32 x, i32 y) const {
        if (!hasClip_) return false;
        return x < clipRect_.x || x >= clipRect_.x + clipRect_.w ||
               y < clipRect_.y || y >= clipRect_.y + clipRect_.h;
    }

    void fillRectScreen(Rect r, Color c) {
        Rect clip = effectiveClip();
        i32 x0 = std::max(i32(r.x), i32(clip.x));
        i32 y0 = std::max(i32(r.y), i32(clip.y));
        i32 x1 = std::min(i32(r.x + r.w), i32(clip.x + clip.w));
        i32 y1 = std::min(i32(r.y + r.h), i32(clip.y + clip.h));
        for (i32 y = y0; y < y1; ++y)
            for (i32 x = x0; x < x1; ++x)
                blendPixel(x, y, c);
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

    void blendPixel(i32 x, i32 y, Color c) {
        if (!target_ || !target_->valid()) return;
        if (x < 0 || x >= target_->width() || y < 0 || y >= target_->height()) return;
        if (isClipped(x, y)) return;

        u32* row = static_cast<u32*>(target_->rowAddr(y));
        u32& pixel = row[x];

        Color dst = decodePixel(pixel);

        u32 invA = 255 - c.a;
        u8 outR = u8((c.r * c.a + dst.r * invA) / 255);
        u8 outG = u8((c.g * c.a + dst.g * invA) / 255);
        u8 outB = u8((c.b * c.a + dst.b * invA) / 255);
        u8 outA = u8((c.a * 255 + dst.a * invA) / 255);

        pixel = encodePixel({outR, outG, outB, outA});
    }
};

} // namespace ink
