#pragma once

/**
 * Stroke-to-fill conversion engine.
 *
 * Internal implementation: converts a stroked path into a filled path by
 * computing offset curves, line joins, and end caps.
 */

#include "ink/path.hpp"
#include <cmath>
#include <vector>

namespace ink {

class StrokeEngine {
public:
  static Path strokePath(const Path &src, f32 width, LineCap cap, LineJoin join,
                         f32 miterLimit) {
    Path result;
    f32 hw = width * 0.5f;
    if (hw <= 0)
      return result;
    if (miterLimit < 1.0f)
      miterLimit = 1.0f;

    // Flatten the path to line segments grouped by contour
    std::vector<std::vector<Point>> contours;
    std::vector<bool> closedFlags;
    flattenToContours(src, contours, closedFlags);

    for (size_t ci = 0; ci < contours.size(); ++ci) {
      const auto &pts = contours[ci];
      bool closed = closedFlags[ci];
      if (pts.size() < 2)
        continue;
      strokeContour(pts, closed, hw, cap, join, miterLimit, result);
    }

    return result;
  }

private:
  static void flattenToContours(const Path &src,
                                std::vector<std::vector<Point>> &contours,
                                std::vector<bool> &closedFlags) {
    const auto &verbs = src.verbs();
    const auto &pts = src.points();
    const auto &cws = src.conicWeights();
    i32 ptIdx = 0, cwIdx = 0;
    Point cur = {0, 0};

    for (auto v : verbs) {
      switch (v) {
      case Path::Verb::Move:
        contours.push_back({});
        closedFlags.push_back(false);
        cur = pts[ptIdx++];
        contours.back().push_back(cur);
        break;
      case Path::Verb::Line: {
        if (contours.empty()) {
          contours.push_back({});
          closedFlags.push_back(false);
        }
        cur = pts[ptIdx++];
        contours.back().push_back(cur);
        break;
      }
      case Path::Verb::Quad: {
        if (contours.empty()) {
          contours.push_back({});
          closedFlags.push_back(false);
        }
        Point cp = pts[ptIdx++], end = pts[ptIdx++];
        flattenQuadInto(contours.back(), cur, cp, end, 0.25f);
        cur = end;
        break;
      }
      case Path::Verb::Cubic: {
        if (contours.empty()) {
          contours.push_back({});
          closedFlags.push_back(false);
        }
        Point c1 = pts[ptIdx++], c2 = pts[ptIdx++], end = pts[ptIdx++];
        flattenCubicInto(contours.back(), cur, c1, c2, end, 0.25f);
        cur = end;
        break;
      }
      case Path::Verb::Conic: {
        if (contours.empty()) {
          contours.push_back({});
          closedFlags.push_back(false);
        }
        Point cp = pts[ptIdx++], end = pts[ptIdx++];
        f32 w = cws[cwIdx++];
        // Approximate conic as quad
        (void)w;
        flattenQuadInto(contours.back(), cur, cp, end, 0.25f);
        cur = end;
        break;
      }
      case Path::Verb::Close:
        if (!contours.empty())
          closedFlags.back() = true;
        break;
      }
    }
  }

  static void flattenQuadInto(std::vector<Point> &out, Point p0, Point p1,
                              Point p2, f32 tol) {
    f32 mx = (p0.x + p2.x) * 0.5f, my = (p0.y + p2.y) * 0.5f;
    f32 dx = p1.x - mx, dy = p1.y - my;
    if (dx * dx + dy * dy <= tol * tol) {
      out.push_back(p2);
      return;
    }
    Point q0 = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
    Point q1 = {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
    Point mid = {(q0.x + q1.x) * 0.5f, (q0.y + q1.y) * 0.5f};
    flattenQuadInto(out, p0, q0, mid, tol);
    flattenQuadInto(out, mid, q1, p2, tol);
  }

  static void flattenCubicInto(std::vector<Point> &out, Point p0, Point p1,
                               Point p2, Point p3, f32 tol) {
    f32 dx = p3.x - p0.x, dy = p3.y - p0.y;
    f32 len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) {
      out.push_back(p3);
      return;
    }
    f32 nx = -dy / len, ny = dx / len;
    f32 d1 = std::abs((p1.x - p0.x) * nx + (p1.y - p0.y) * ny);
    f32 d2 = std::abs((p2.x - p0.x) * nx + (p2.y - p0.y) * ny);
    if (std::max(d1, d2) <= tol) {
      out.push_back(p3);
      return;
    }
    Point ab = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
    Point bc = {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
    Point cd = {(p2.x + p3.x) * 0.5f, (p2.y + p3.y) * 0.5f};
    Point abc = {(ab.x + bc.x) * 0.5f, (ab.y + bc.y) * 0.5f};
    Point bcd = {(bc.x + cd.x) * 0.5f, (bc.y + cd.y) * 0.5f};
    Point mid = {(abc.x + bcd.x) * 0.5f, (abc.y + bcd.y) * 0.5f};
    flattenCubicInto(out, p0, ab, abc, mid, tol);
    flattenCubicInto(out, mid, bcd, cd, p3, tol);
  }

  static Point normalize(Point v) {
    f32 len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len < 1e-8f)
      return {0, 0};
    return {v.x / len, v.y / len};
  }

  static Point perp(Point v) { return {-v.y, v.x}; }

  static f32 dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }
  static f32 cross(Point a, Point b) { return a.x * b.y - a.y * b.x; }

  static void strokeContour(const std::vector<Point> &pts, bool closed, f32 hw,
                            LineCap cap, LineJoin join, f32 miterLimit,
                            Path &out) {
    i32 n = static_cast<i32>(pts.size());
    if (n < 2)
      return;

    // Compute per-segment directions and normals
    std::vector<Point> dirs(n - 1), norms(n - 1);
    for (i32 i = 0; i < n - 1; ++i) {
      dirs[i] = normalize({pts[i + 1].x - pts[i].x, pts[i + 1].y - pts[i].y});
      norms[i] = perp(dirs[i]);
    }

    // Build left and right offset polylines
    std::vector<Point> leftPts, rightPts;

    if (!closed) {
      addCap(pts[0], {-dirs[0].x, -dirs[0].y}, norms[0], hw, cap, leftPts,
             rightPts, true);
    }

    for (i32 i = 0; i < n - 1; ++i) {
      Point p = pts[i];
      Point lp = {p.x + norms[i].x * hw, p.y + norms[i].y * hw};
      Point rp = {p.x - norms[i].x * hw, p.y - norms[i].y * hw};

      if (i == 0 && !closed) {
        leftPts.push_back(lp);
        rightPts.push_back(rp);
      }

      Point pEnd = pts[i + 1];
      Point lpEnd = {pEnd.x + norms[i].x * hw, pEnd.y + norms[i].y * hw};
      Point rpEnd = {pEnd.x - norms[i].x * hw, pEnd.y - norms[i].y * hw};

      if (i + 1 < n - 1 || closed) {
        i32 nextI = (i + 1) % (n - 1);
        if (i + 1 == n - 1 && !closed) {
          leftPts.push_back(lpEnd);
          rightPts.push_back(rpEnd);
        } else {
          addJoin(lpEnd, rpEnd, pEnd, norms[i], norms[nextI], dirs[i],
                  dirs[nextI], hw, join, miterLimit, leftPts, rightPts);
        }
      } else {
        leftPts.push_back(lpEnd);
        rightPts.push_back(rpEnd);
      }
    }

    if (!closed) {
      addCap(pts[n - 1], dirs[n - 2], norms[n - 2], hw, cap, leftPts, rightPts,
             false);
    }

    // Construct output path
    if (leftPts.empty())
      return;
    out.moveTo(leftPts[0]);
    for (size_t i = 1; i < leftPts.size(); ++i)
      out.lineTo(leftPts[i]);

    if (closed) {
      out.close();
      out.moveTo(rightPts[0]);
      for (size_t i = 1; i < rightPts.size(); ++i)
        out.lineTo(rightPts[i]);
      out.close();
    } else {
      // Connect right side in reverse
      for (i32 i = static_cast<i32>(rightPts.size()) - 1; i >= 0; --i)
        out.lineTo(rightPts[i]);
      out.close();
    }
  }

  static void addJoin(Point lp, Point rp, Point pivot, Point norm0, Point norm1,
                      Point dir0, Point dir1, f32 hw, LineJoin join,
                      f32 miterLimit, std::vector<Point> &left,
                      std::vector<Point> &right) {
    Point ln = {pivot.x + norm1.x * hw, pivot.y + norm1.y * hw};
    Point rn = {pivot.x - norm1.x * hw, pivot.y - norm1.y * hw};

    f32 crossVal = cross(dir0, dir1);

    if (join == LineJoin::Bevel || std::abs(crossVal) < 1e-6f) {
      left.push_back(lp);
      left.push_back(ln);
      right.push_back(rp);
      right.push_back(rn);
      return;
    }

    if (join == LineJoin::Miter) {
      f32 sinHalf = std::abs(crossVal) * 0.5f;
      if (sinHalf > 0 && 1.0f / sinHalf <= miterLimit) {
        // Compute miter point
        Point miterL = intersectLines(lp, dir0, ln, dir1);
        Point miterR = intersectLines(rp, dir0, rn, dir1);
        left.push_back(miterL);
        right.push_back(miterR);
      } else {
        // Fall back to bevel
        left.push_back(lp);
        left.push_back(ln);
        right.push_back(rp);
        right.push_back(rn);
      }
      return;
    }

    // Round join
    left.push_back(lp);
    right.push_back(rp);

    f32 angle0 = std::atan2(norm0.y, norm0.x);
    f32 angle1 = std::atan2(norm1.y, norm1.x);
    f32 sweep = angle1 - angle0;
    constexpr f32 kPI2 = 6.28318530718f;
    if (sweep > 3.14159265f)
      sweep -= kPI2;
    if (sweep < -3.14159265f)
      sweep += kPI2;

    i32 steps = std::max(
        2, static_cast<i32>(std::ceil(std::abs(sweep) * 8.0f / 3.14159265f)));
    f32 step = sweep / steps;
    for (i32 s = 1; s < steps; ++s) {
      f32 a = angle0 + s * step;
      f32 ca = std::cos(a), sa = std::sin(a);
      left.push_back({pivot.x + ca * hw, pivot.y + sa * hw});
      right.push_back({pivot.x - ca * hw, pivot.y - sa * hw});
    }
    left.push_back(ln);
    right.push_back(rn);
  }

  static Point intersectLines(Point p0, Point d0, Point p1, Point d1) {
    f32 denom = cross(d0, d1);
    if (std::abs(denom) < 1e-10f)
      return p0;
    f32 t = cross({p1.x - p0.x, p1.y - p0.y}, d1) / denom;
    return {p0.x + d0.x * t, p0.y + d0.y * t};
  }

  static void addCap(Point pt, Point dir, Point norm, f32 hw, LineCap cap,
                     std::vector<Point> &left, std::vector<Point> &right,
                     bool isStart) {
    Point lp = {pt.x + norm.x * hw, pt.y + norm.y * hw};
    Point rp = {pt.x - norm.x * hw, pt.y - norm.y * hw};

    switch (cap) {
    case LineCap::Butt:
      if (isStart) {
        left.push_back(lp);
        right.push_back(rp);
      } else {
        left.push_back(lp);
        right.push_back(rp);
      }
      break;
    case LineCap::Square: {
      Point ext = {dir.x * hw, dir.y * hw};
      if (isStart) {
        left.push_back({lp.x - ext.x, lp.y - ext.y});
        right.push_back({rp.x - ext.x, rp.y - ext.y});
      } else {
        left.push_back({lp.x + ext.x, lp.y + ext.y});
        right.push_back({rp.x + ext.x, rp.y + ext.y});
      }
      break;
    }
    case LineCap::Round: {
      constexpr i32 kSteps = 8;
      f32 startAngle = std::atan2(norm.y, norm.x);
      f32 sweep = isStart ? 3.14159265f : -3.14159265f;
      f32 step = sweep / kSteps;
      for (i32 i = 0; i <= kSteps; ++i) {
        f32 a = startAngle + (isStart ? 3.14159265f : 0) + i * step;
        left.push_back({pt.x + std::cos(a) * hw, pt.y + std::sin(a) * hw});
      }
      if (isStart) {
        right.push_back(rp);
      } else {
        right.push_back(rp);
      }
      break;
    }
    }
  }
};

} // namespace ink
