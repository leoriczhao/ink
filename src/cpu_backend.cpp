#include "ink/cpu_backend.hpp"
#include "ink/glyph_cache.hpp"
#include "ink/image.hpp"
#include <algorithm>
#include <cmath>

namespace ink {

CpuBackend::CpuBackend(Pixmap* target)
    : target_(target) {
}

void CpuBackend::beginFrame() {
    if (target_ && target_->valid()) {
        target_->clear({0, 0, 0, 255});
    }
    hasClip_ = false;
    clipRect_ = {};
}

void CpuBackend::endFrame() {
}

void CpuBackend::resize(i32, i32) {
    // Pixmap resize is managed by Surface
}

void CpuBackend::execute(const Recording& recording, const DrawPass& pass) {
    const auto& ops = recording.ops();
    const auto& arena = recording.arena();

    for (u32 idx : pass.sortedIndices()) {
        const auto& op = ops[idx];

        switch (op.type) {
        case DrawOp::Type::FillRect:
            execFillRect(op.data.fill.rect, op.color);
            break;
        case DrawOp::Type::StrokeRect:
            execStrokeRect(op.data.stroke.rect, op.color, op.width);
            break;
        case DrawOp::Type::Line:
            execLine(op.data.line.p1, op.data.line.p2, op.color, op.width);
            break;
        case DrawOp::Type::Polyline:
            execPolyline(arena.getPoints(op.data.polyline.offset),
                         i32(op.data.polyline.count), op.color, op.width);
            break;
        case DrawOp::Type::Text:
            execText(op.data.text.pos,
                     arena.getString(op.data.text.offset),
                     op.data.text.len, op.color);
            break;
        case DrawOp::Type::DrawImage:
            execDrawImage(recording.getImage(op.data.image.imageIndex),
                          op.data.image.x, op.data.image.y);
            break;
        case DrawOp::Type::SetClip:
            execSetClip(op.data.clip.rect);
            break;
        case DrawOp::Type::ClearClip:
            execClearClip();
            break;
        }
    }
}

// --- Clip helpers ---

Rect CpuBackend::effectiveClip() const {
    if (!target_ || !target_->valid()) return {};
    if (hasClip_) return clipRect_;
    return {0, 0, f32(target_->width()), f32(target_->height())};
}

bool CpuBackend::isClipped(i32 x, i32 y) const {
    Rect clip = effectiveClip();
    return x < i32(clip.x) || x >= i32(clip.x + clip.w) ||
           y < i32(clip.y) || y >= i32(clip.y + clip.h);
}

// --- Pixel-level rendering ---

void CpuBackend::blendPixel(i32 x, i32 y, Color c) {
    if (!target_ || !target_->valid()) return;
    if (x < 0 || x >= target_->width() || y < 0 || y >= target_->height()) return;
    if (isClipped(x, y)) return;

    u32* row = static_cast<u32*>(target_->rowAddr(y));
    u32& dst = row[x];

    if (c.a == 255) {
        if (target_->format() == PixelFormat::BGRA8888) {
            dst = 0xFF000000 | (u32(c.r) << 16) | (u32(c.g) << 8) | u32(c.b);
        } else {
            dst = 0xFF000000 | (u32(c.b) << 16) | (u32(c.g) << 8) | u32(c.r);
        }
        return;
    }

    if (c.a == 0) return;

    u8 dstR, dstG, dstB;
    if (target_->format() == PixelFormat::BGRA8888) {
        dstR = (dst >> 16) & 0xFF;
        dstG = (dst >> 8) & 0xFF;
        dstB = dst & 0xFF;
    } else {
        dstR = dst & 0xFF;
        dstG = (dst >> 8) & 0xFF;
        dstB = (dst >> 16) & 0xFF;
    }

    u8 a = c.a;
    u8 outR = u8((c.r * a + dstR * (255 - a)) / 255);
    u8 outG = u8((c.g * a + dstG * (255 - a)) / 255);
    u8 outB = u8((c.b * a + dstB * (255 - a)) / 255);

    if (target_->format() == PixelFormat::BGRA8888) {
        dst = 0xFF000000 | (u32(outR) << 16) | (u32(outG) << 8) | u32(outB);
    } else {
        dst = 0xFF000000 | (u32(outB) << 16) | (u32(outG) << 8) | u32(outR);
    }
}

void CpuBackend::drawHLine(i32 x1, i32 x2, i32 y, Color c) {
    if (!target_ || !target_->valid()) return;
    if (y < 0 || y >= target_->height()) return;

    Rect clip = effectiveClip();
    x1 = std::max(x1, i32(clip.x));
    x2 = std::min(x2, i32(clip.x + clip.w) - 1);
    if (y < i32(clip.y) || y >= i32(clip.y + clip.h)) return;
    if (x1 > x2) return;

    x1 = std::max(x1, 0);
    x2 = std::min(x2, target_->width() - 1);

    u32* row = static_cast<u32*>(target_->rowAddr(y));
    u32 pixel;
    if (target_->format() == PixelFormat::BGRA8888) {
        pixel = (u32(c.a) << 24) | (u32(c.r) << 16) | (u32(c.g) << 8) | u32(c.b);
    } else {
        pixel = (u32(c.a) << 24) | (u32(c.b) << 16) | (u32(c.g) << 8) | u32(c.r);
    }

    if (c.a == 255) {
        for (i32 x = x1; x <= x2; ++x) {
            row[x] = pixel;
        }
    } else {
        for (i32 x = x1; x <= x2; ++x) {
            blendPixel(x, y, c);
        }
    }
}

void CpuBackend::drawLineImpl(i32 x1, i32 y1, i32 x2, i32 y2, Color c) {
    // Bresenham's line algorithm
    i32 dx = std::abs(x2 - x1);
    i32 dy = std::abs(y2 - y1);
    i32 sx = x1 < x2 ? 1 : -1;
    i32 sy = y1 < y2 ? 1 : -1;
    i32 err = dx - dy;

    while (true) {
        blendPixel(x1, y1, c);
        if (x1 == x2 && y1 == y2) break;
        i32 e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void CpuBackend::drawImageImpl(const Image* image, f32 x, f32 y) {
    if (!target_ || !target_->valid()) return;
    if (!image || !image->valid()) return;

    i32 dstX = i32(x);
    i32 dstY = i32(y);
    i32 srcW = image->width();
    i32 srcH = image->height();

    // Compute clipped source/dest regions
    Rect clip = effectiveClip();
    i32 clipX1 = std::max(i32(clip.x), 0);
    i32 clipY1 = std::max(i32(clip.y), 0);
    i32 clipX2 = std::min(i32(clip.x + clip.w), target_->width());
    i32 clipY2 = std::min(i32(clip.y + clip.h), target_->height());

    i32 startX = std::max(dstX, clipX1);
    i32 startY = std::max(dstY, clipY1);
    i32 endX = std::min(dstX + srcW, clipX2);
    i32 endY = std::min(dstY + srcH, clipY2);

    if (startX >= endX || startY >= endY) return;

    i32 srcStridePixels = image->stride() / 4;
    const u32* srcPixels = image->pixels32();
    bool sameFmt = (image->format() == target_->format());

    for (i32 row = startY; row < endY; ++row) {
        i32 srcRow = row - dstY;
        const u32* srcLine = srcPixels + srcRow * srcStridePixels;
        u32* dstLine = static_cast<u32*>(target_->rowAddr(row));

        for (i32 col = startX; col < endX; ++col) {
            i32 srcCol = col - dstX;
            u32 sp = srcLine[srcCol];

            u8 sa = (sp >> 24) & 0xFF;
            if (sa == 0) continue;

            // Extract source RGBA from the image's pixel format
            u8 sr, sg, sb;
            if (image->format() == PixelFormat::BGRA8888) {
                sr = (sp >> 16) & 0xFF;
                sg = (sp >> 8) & 0xFF;
                sb = sp & 0xFF;
            } else {
                sr = sp & 0xFF;
                sg = (sp >> 8) & 0xFF;
                sb = (sp >> 16) & 0xFF;
            }

            if (sa == 255) {
                // Opaque: write directly in target format
                if (target_->format() == PixelFormat::BGRA8888) {
                    dstLine[col] = 0xFF000000 | (u32(sr) << 16) | (u32(sg) << 8) | u32(sb);
                } else {
                    dstLine[col] = 0xFF000000 | (u32(sb) << 16) | (u32(sg) << 8) | u32(sr);
                }
            } else {
                // Alpha blend
                blendPixel(col, row, {sr, sg, sb, sa});
            }
        }
    }
}

// --- Exec methods (dispatch from execute) ---

void CpuBackend::execFillRect(Rect r, Color c) {
    if (!target_ || !target_->valid()) return;

    Rect clip = effectiveClip();
    i32 x1 = std::max(i32(r.x), i32(clip.x));
    i32 y1 = std::max(i32(r.y), i32(clip.y));
    i32 x2 = std::min(i32(r.x + r.w), i32(clip.x + clip.w));
    i32 y2 = std::min(i32(r.y + r.h), i32(clip.y + clip.h));

    x1 = std::max(x1, 0);
    y1 = std::max(y1, 0);
    x2 = std::min(x2, target_->width());
    y2 = std::min(y2, target_->height());

    if (x1 >= x2 || y1 >= y2) return;

    for (i32 y = y1; y < y2; ++y) {
        drawHLine(x1, x2 - 1, y, c);
    }
}

void CpuBackend::execStrokeRect(Rect r, Color c, f32) {
    if (!target_ || !target_->valid()) return;

    i32 x1 = i32(r.x);
    i32 y1 = i32(r.y);
    i32 x2 = i32(r.x + r.w);
    i32 y2 = i32(r.y + r.h);

    drawHLine(x1, x2, y1, c);
    drawHLine(x1, x2, y2, c);
    for (i32 y = y1; y <= y2; ++y) {
        blendPixel(x1, y, c);
        blendPixel(x2, y, c);
    }
}

void CpuBackend::execLine(Point p1, Point p2, Color c, f32) {
    drawLineImpl(i32(p1.x), i32(p1.y), i32(p2.x), i32(p2.y), c);
}

void CpuBackend::execPolyline(const Point* pts, i32 count, Color c, f32) {
    for (i32 i = 0; i + 1 < count; ++i) {
        drawLineImpl(i32(pts[i].x), i32(pts[i].y),
                     i32(pts[i + 1].x), i32(pts[i + 1].y), c);
    }
}

void CpuBackend::execText(Point p, const char* text, u32 len, Color c) {
    if (!target_ || !target_->valid() || !glyphCache_) return;

    glyphCache_->drawText(target_->addr32(), target_->width(), target_->height(),
                          i32(p.x), i32(p.y),
                          std::string_view(text, len), c);
}

void CpuBackend::execDrawImage(const Image* image, f32 x, f32 y) {
    drawImageImpl(image, x, y);
}

void CpuBackend::execSetClip(Rect r) {
    clipRect_ = r;
    hasClip_ = true;
}

void CpuBackend::execClearClip() {
    hasClip_ = false;
    clipRect_ = {};
}

} // namespace ink
