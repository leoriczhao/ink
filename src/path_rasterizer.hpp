#pragma once

/**
 * Scanline path rasterizer with optional anti-aliasing.
 *
 * Internal implementation used by CpuRenderer. Converts a Path to a set of
 * edges, then sweeps scanlines to fill pixels according to the fill rule.
 *
 * AA is achieved via 8x supersampling in Y: each pixel row is divided into
 * 8 sub-scanlines, and coverage is accumulated per pixel.
 */

#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include "ink/path.hpp"
#include "ink/pixmap.hpp"
#include "ink/shader.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace ink {

struct RastEdge {
  f32 yTop, yBot;
  f32 xAtTop;
  f32 dxPerY;
  i32 winding;
};

class PathRasterizer {
public:
  void reset() { edges_.clear(); }

  void addPath(const Path &path, const Matrix &transform) {
    const auto &verbs = path.verbs();
    const auto &pts = path.points();
    const auto &cws = path.conicWeights();
    i32 ptIdx = 0, cwIdx = 0;
    Point contourStart = {0, 0}, cur = {0, 0};

    for (auto v : verbs) {
      switch (v) {
      case Path::Verb::Move:
        contourStart = cur = transform.mapPoint(pts[ptIdx++]);
        break;
      case Path::Verb::Line: {
        Point next = transform.mapPoint(pts[ptIdx++]);
        addLine(cur, next);
        cur = next;
        break;
      }
      case Path::Verb::Quad: {
        Point cp = transform.mapPoint(pts[ptIdx++]);
        Point end = transform.mapPoint(pts[ptIdx++]);
        flattenQuad(cur, cp, end, kFlatTolerance);
        cur = end;
        break;
      }
      case Path::Verb::Cubic: {
        Point c1 = transform.mapPoint(pts[ptIdx++]);
        Point c2 = transform.mapPoint(pts[ptIdx++]);
        Point end = transform.mapPoint(pts[ptIdx++]);
        flattenCubic(cur, c1, c2, end, kFlatTolerance);
        cur = end;
        break;
      }
      case Path::Verb::Conic: {
        Point cp = transform.mapPoint(pts[ptIdx++]);
        Point end = transform.mapPoint(pts[ptIdx++]);
        f32 w = cws[cwIdx++];
        flattenConic(cur, cp, end, w, kFlatTolerance);
        cur = end;
        break;
      }
      case Path::Verb::Close:
        addLine(cur, contourStart);
        cur = contourStart;
        break;
      }
    }
  }

  void rasterize(Pixmap *target, const Rect &clip, Color color, BlendMode blend,
                 u8 opacity, bool antialias, FillRule fillRule,
                 const Shader *shader, bool isBGRA,
                 const u8 *clipMask = nullptr, i32 clipMaskStride = 0) {
    if (edges_.empty() || !target || !target->valid())
      return;

    color.a = static_cast<u8>(color.a * opacity / 255);

    i32 clipX0 = std::max(0, static_cast<i32>(clip.x));
    i32 clipY0 = std::max(0, static_cast<i32>(clip.y));
    i32 clipX1 = std::min(target->width(), static_cast<i32>(clip.x + clip.w));
    i32 clipY1 = std::min(target->height(), static_cast<i32>(clip.y + clip.h));
    if (clipX0 >= clipX1 || clipY0 >= clipY1)
      return;

    i32 width = clipX1 - clipX0;
    std::vector<f32> coverage(width, 0.0f);
    std::vector<Color> shaderColors;
    if (shader)
      shaderColors.resize(width);

    i32 subCount = antialias ? kAAFactor : 1;
    f32 subStep = 1.0f / subCount;

    for (i32 y = clipY0; y < clipY1; ++y) {
      std::fill(coverage.begin(), coverage.end(), 0.0f);

      for (i32 sub = 0; sub < subCount; ++sub) {
        f32 scanY = y + (sub + 0.5f) * subStep;
        fillScanline(scanY, clipX0, clipX1, fillRule, coverage.data(), subStep);
      }

      if (shader) {
        shader->shadeSpan(clipX0, y, width, shaderColors.data());
      }

      u32 *row = static_cast<u32 *>(target->rowAddr(y));
      for (i32 i = 0; i < width; ++i) {
        f32 cov = coverage[i];
        if (cov <= 0)
          continue;
        if (cov > 1.0f)
          cov = 1.0f;

        Color src = shader ? shaderColors[i] : color;
        u8 covAlpha = static_cast<u8>(cov * 255.0f + 0.5f);
        src.a = static_cast<u8>(src.a * covAlpha / 255);

        i32 px = clipX0 + i;
        if (clipMask) {
          u8 maskAlpha = clipMask[y * clipMaskStride + px];
          if (maskAlpha == 0)
            continue;
          src.a = static_cast<u8>(src.a * maskAlpha / 255);
        }
        u32 &pixel = row[px];
        Color dst = decodePixel(pixel, isBGRA);
        Color out = blendColors(src, dst, blend);
        pixel = encodePixel(out, isBGRA);
      }
    }
  }

private:
  static constexpr f32 kFlatTolerance = 0.25f;
  static constexpr i32 kAAFactor = 8;

  std::vector<RastEdge> edges_;

  void addLine(Point p0, Point p1) {
    if (std::abs(p0.y - p1.y) < 0.001f)
      return;
    RastEdge e;
    if (p0.y < p1.y) {
      e.yTop = p0.y;
      e.yBot = p1.y;
      e.xAtTop = p0.x;
      e.winding = 1;
    } else {
      e.yTop = p1.y;
      e.yBot = p0.y;
      e.xAtTop = p1.x;
      e.winding = -1;
    }
    e.dxPerY = (p1.x - p0.x) / (p1.y - p0.y);
    edges_.push_back(e);
  }

  void flattenQuad(Point p0, Point p1, Point p2, f32 tol) {
    f32 mx = (p0.x + p2.x) * 0.5f, my = (p0.y + p2.y) * 0.5f;
    f32 dx = p1.x - mx, dy = p1.y - my;
    if (dx * dx + dy * dy <= tol * tol) {
      addLine(p0, p2);
      return;
    }
    Point q0 = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
    Point q1 = {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
    Point mid = {(q0.x + q1.x) * 0.5f, (q0.y + q1.y) * 0.5f};
    flattenQuad(p0, q0, mid, tol);
    flattenQuad(mid, q1, p2, tol);
  }

  void flattenCubic(Point p0, Point p1, Point p2, Point p3, f32 tol) {
    f32 dx = p3.x - p0.x, dy = p3.y - p0.y;
    f32 len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) {
      addLine(p0, p3);
      return;
    }
    f32 nx = -dy / len, ny = dx / len;
    f32 d1 = std::abs((p1.x - p0.x) * nx + (p1.y - p0.y) * ny);
    f32 d2 = std::abs((p2.x - p0.x) * nx + (p2.y - p0.y) * ny);
    if (std::max(d1, d2) <= tol) {
      addLine(p0, p3);
      return;
    }
    Point ab = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
    Point bc = {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
    Point cd = {(p2.x + p3.x) * 0.5f, (p2.y + p3.y) * 0.5f};
    Point abc = {(ab.x + bc.x) * 0.5f, (ab.y + bc.y) * 0.5f};
    Point bcd = {(bc.x + cd.x) * 0.5f, (bc.y + cd.y) * 0.5f};
    Point mid = {(abc.x + bcd.x) * 0.5f, (abc.y + bcd.y) * 0.5f};
    flattenCubic(p0, ab, abc, mid, tol);
    flattenCubic(mid, bcd, cd, p3, tol);
  }

  void flattenConic(Point p0, Point p1, Point p2, f32 w, f32 tol) {
    // Subdivide conic into quads using standard decomposition
    if (std::abs(w - 1.0f) < 0.01f) {
      flattenQuad(p0, p1, p2, tol);
      return;
    }
    // Split at t=0.5 using conic subdivision formula
    f32 ww = w;
    f32 scale = 1.0f / (1.0f + ww);
    Point q0 = {(p0.x + ww * p1.x) * scale, (p0.y + ww * p1.y) * scale};
    Point q2 = {(ww * p1.x + p2.x) * scale, (ww * p1.y + p2.y) * scale};
    Point mid = {(q0.x + q2.x) * 0.5f, (q0.y + q2.y) * 0.5f};
    f32 newW = std::sqrt(0.5f + ww * 0.5f);

    f32 mx = (p0.x + p2.x) * 0.5f, my = (p0.y + p2.y) * 0.5f;
    f32 dx = mid.x - mx, dy = mid.y - my;
    if (dx * dx + dy * dy <= tol * tol) {
      addLine(p0, mid);
      addLine(mid, p2);
      return;
    }
    flattenConic(p0, q0, mid, newW, tol);
    flattenConic(mid, q2, p2, newW, tol);
  }

  struct XIntersect {
    f32 x;
    i32 winding;
  };

  void fillScanline(f32 scanY, i32 clipX0, i32 clipX1, FillRule fillRule,
                    f32 *coverage, f32 weight) {
    thread_local std::vector<XIntersect> xInts;
    xInts.clear();

    for (const auto &e : edges_) {
      if (scanY < e.yTop || scanY >= e.yBot)
        continue;
      f32 x = e.xAtTop + (scanY - e.yTop) * e.dxPerY;
      xInts.push_back({x, e.winding});
    }

    if (xInts.empty())
      return;
    std::sort(
        xInts.begin(), xInts.end(),
        [](const XIntersect &a, const XIntersect &b) { return a.x < b.x; });

    i32 winding = 0;
    for (size_t i = 0; i < xInts.size(); ++i) {
      i32 prevWinding = winding;
      winding += xInts[i].winding;

      bool wasFilled = (fillRule == FillRule::EvenOdd) ? (prevWinding & 1) != 0
                                                       : prevWinding != 0;
      bool isFilled =
          (fillRule == FillRule::EvenOdd) ? (winding & 1) != 0 : winding != 0;

      if (!wasFilled && isFilled && i + 1 < xInts.size()) {
        // Find the end of this filled span
        f32 spanStart = xInts[i].x;
        f32 spanEnd = spanStart;
        for (size_t j = i + 1; j < xInts.size(); ++j) {
          i32 nextWinding = winding;
          spanEnd = xInts[j].x;
          nextWinding += xInts[j].winding;
          bool nextFilled = (fillRule == FillRule::EvenOdd)
                                ? (nextWinding & 1) != 0
                                : nextWinding != 0;
          if (!nextFilled) {
            // Fill the span [spanStart, spanEnd]
            fillSpan(spanStart, spanEnd, clipX0, clipX1, coverage, weight);
            winding = nextWinding;
            i = j;
            break;
          }
          winding = nextWinding;
          i = j;
          if (j + 1 >= xInts.size()) {
            fillSpan(spanStart, spanEnd, clipX0, clipX1, coverage, weight);
          }
        }
      }
    }
  }

  void fillSpan(f32 x0, f32 x1, i32 clipX0, i32 clipX1, f32 *coverage,
                f32 weight) {
    if (x0 >= x1)
      return;
    f32 fx0 = std::max(x0, static_cast<f32>(clipX0));
    f32 fx1 = std::min(x1, static_cast<f32>(clipX1));
    if (fx0 >= fx1)
      return;

    i32 ix0 = static_cast<i32>(std::floor(fx0));
    i32 ix1 = static_cast<i32>(std::ceil(fx1));
    ix0 = std::max(ix0, clipX0);
    ix1 = std::min(ix1, clipX1);

    for (i32 px = ix0; px < ix1; ++px) {
      f32 left = std::max(fx0, static_cast<f32>(px));
      f32 right = std::min(fx1, static_cast<f32>(px + 1));
      f32 frac = right - left;
      coverage[px - clipX0] += frac * weight;
    }
  }

  static Color decodePixel(u32 raw, bool isBGRA) {
    if (isBGRA) {
      return {static_cast<u8>((raw >> 16) & 0xFF),
              static_cast<u8>((raw >> 8) & 0xFF), static_cast<u8>(raw & 0xFF),
              static_cast<u8>((raw >> 24) & 0xFF)};
    }
    return {static_cast<u8>(raw & 0xFF), static_cast<u8>((raw >> 8) & 0xFF),
            static_cast<u8>((raw >> 16) & 0xFF),
            static_cast<u8>((raw >> 24) & 0xFF)};
  }

  static u32 encodePixel(Color c, bool isBGRA) {
    if (isBGRA) {
      return (u32(c.a) << 24) | (u32(c.r) << 16) | (u32(c.g) << 8) | u32(c.b);
    }
    return (u32(c.a) << 24) | (u32(c.b) << 16) | (u32(c.g) << 8) | u32(c.r);
  }

  static Color blendColors(Color src, Color dst, BlendMode mode) {
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
    return {
        static_cast<u8>((src.r * sa * fa / 255 + dst.r * da * fb / 255) / 255),
        static_cast<u8>((src.g * sa * fa / 255 + dst.g * da * fb / 255) / 255),
        static_cast<u8>((src.b * sa * fa / 255 + dst.b * da * fb / 255) / 255),
        static_cast<u8>((sa * fa + da * fb) / 255),
    };
  }
};

} // namespace ink
