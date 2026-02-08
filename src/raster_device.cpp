#include "ink/raster_device.hpp"
#include "ink/glyph_cache.hpp"
#include <algorithm>
#include <cmath>

namespace ink {

SoftwareRasterDevice::SoftwareRasterDevice(Pixmap* target)
    : target_(target) {
}

void SoftwareRasterDevice::resize(i32, i32) {
    // Pixmap resize is managed by Surface
}

void SoftwareRasterDevice::beginFrame() {
    if (target_ && target_->valid()) {
        target_->clear({0, 0, 0, 255});
    }
}

void SoftwareRasterDevice::endFrame() {
}

Rect SoftwareRasterDevice::effectiveClip() const {
    if (!target_ || !target_->valid()) return {};
    if (hasClip_) return clipRect_;
    return {0, 0, f32(target_->width()), f32(target_->height())};
}

bool SoftwareRasterDevice::isClipped(i32 x, i32 y) const {
    Rect clip = effectiveClip();
    return x < i32(clip.x) || x >= i32(clip.x + clip.w) ||
           y < i32(clip.y) || y >= i32(clip.y + clip.h);
}

void SoftwareRasterDevice::blendPixel(i32 x, i32 y, Color c) {
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

void SoftwareRasterDevice::drawHLine(i32 x1, i32 x2, i32 y, Color c) {
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

void SoftwareRasterDevice::fillRect(Rect r, Color c) {
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

void SoftwareRasterDevice::strokeRect(Rect r, Color c, f32) {
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

void SoftwareRasterDevice::drawLineImpl(i32 x1, i32 y1, i32 x2, i32 y2, Color c) {
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

void SoftwareRasterDevice::drawLine(Point p1, Point p2, Color c, f32) {
    drawLineImpl(i32(p1.x), i32(p1.y), i32(p2.x), i32(p2.y), c);
}

void SoftwareRasterDevice::drawPolyline(const Point* pts, i32 count, Color c, f32) {
    for (i32 i = 0; i + 1 < count; ++i) {
        drawLineImpl(i32(pts[i].x), i32(pts[i].y),
                     i32(pts[i + 1].x), i32(pts[i + 1].y), c);
    }
}

void SoftwareRasterDevice::drawText(Point p, std::string_view text, Color c) {
    if (!target_ || !target_->valid() || !glyphCache_) return;

    glyphCache_->drawText(target_->addr32(), target_->width(), target_->height(),
                          i32(p.x), i32(p.y), text, c);
}

void SoftwareRasterDevice::setClipRect(Rect r) {
    clipRect_ = r;
    hasClip_ = true;
}

void SoftwareRasterDevice::resetClip() {
    hasClip_ = false;
    clipRect_ = {};
}

}
