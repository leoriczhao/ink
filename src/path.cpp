#include "ink/path.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ink {

static constexpr f32 kPI = 3.14159265358979323846f;
static constexpr f32 kFlatTolerance = 0.25f;

namespace {

struct FlatContour {
  std::vector<Point> points;
  bool closed = false;
};

static f32 distToLineSq(Point p, Point a, Point b) {
  f32 dx = b.x - a.x;
  f32 dy = b.y - a.y;
  f32 lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-12f) {
    f32 px = p.x - a.x;
    f32 py = p.y - a.y;
    return px * px + py * py;
  }
  f32 cross = (p.x - a.x) * dy - (p.y - a.y) * dx;
  return (cross * cross) / lenSq;
}

static void flattenQuadInto(std::vector<Point> &out, Point p0, Point p1,
                            Point p2, f32 tol) {
  if (distToLineSq(p1, p0, p2) <= tol * tol) {
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
  f32 d1 = distToLineSq(p1, p0, p3);
  f32 d2 = distToLineSq(p2, p0, p3);
  if (std::max(d1, d2) <= tol * tol) {
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

static Point evalConic(Point p0, Point p1, Point p2, f32 w, f32 t) {
  f32 mt = 1.0f - t;
  f32 a = mt * mt;
  f32 b = 2.0f * w * mt * t;
  f32 c = t * t;
  f32 denom = a + b + c;
  if (std::abs(denom) < 1e-8f)
    return p2;
  return {
      (a * p0.x + b * p1.x + c * p2.x) / denom,
      (a * p0.y + b * p1.y + c * p2.y) / denom,
  };
}

static void flattenConicInto(std::vector<Point> &out, Point p0, Point p1,
                             Point p2, f32 w, f32 tol) {
  i32 steps =
      std::max(4, static_cast<i32>(std::ceil(
                      std::sqrt(std::max(distToLineSq(p1, p0, p2), 1.0f)) /
                      std::max(tol, 0.01f))));
  steps = std::min(steps, 64);
  for (i32 i = 1; i <= steps; ++i) {
    out.push_back(evalConic(p0, p1, p2, w, static_cast<f32>(i) / steps));
  }
}

static std::vector<FlatContour> flattenPath(const Path &path,
                                            f32 tol = kFlatTolerance) {
  std::vector<FlatContour> contours;
  const auto &verbs = path.verbs();
  const auto &pts = path.points();
  const auto &weights = path.conicWeights();
  i32 ptIdx = 0;
  i32 weightIdx = 0;
  Point cur = {0, 0};

  for (auto verb : verbs) {
    switch (verb) {
    case Path::Verb::Move:
      contours.push_back({});
      cur = pts[ptIdx++];
      contours.back().points.push_back(cur);
      break;
    case Path::Verb::Line: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      cur = pts[ptIdx++];
      contours.back().points.push_back(cur);
      break;
    }
    case Path::Verb::Quad: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      Point cp = pts[ptIdx++];
      Point end = pts[ptIdx++];
      flattenQuadInto(contours.back().points, cur, cp, end, tol);
      cur = end;
      break;
    }
    case Path::Verb::Cubic: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      Point c1 = pts[ptIdx++];
      Point c2 = pts[ptIdx++];
      Point end = pts[ptIdx++];
      flattenCubicInto(contours.back().points, cur, c1, c2, end, tol);
      cur = end;
      break;
    }
    case Path::Verb::Conic: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      Point cp = pts[ptIdx++];
      Point end = pts[ptIdx++];
      f32 w = weights[weightIdx++];
      flattenConicInto(contours.back().points, cur, cp, end, w, tol);
      cur = end;
      break;
    }
    case Path::Verb::Close:
      if (!contours.empty())
        contours.back().closed = true;
      break;
    }
  }

  contours.erase(
      std::remove_if(contours.begin(), contours.end(),
                     [](const FlatContour &c) { return c.points.empty(); }),
      contours.end());
  return contours;
}

static f32 signedArea(const FlatContour &contour) {
  const auto &pts = contour.points;
  if (pts.size() < 3)
    return 0.0f;
  f32 area = 0.0f;
  for (size_t i = 0; i < pts.size(); ++i) {
    const Point &a = pts[i];
    const Point &b = pts[(i + 1) % pts.size()];
    area += a.x * b.y - b.x * a.y;
  }
  return area * 0.5f;
}

} // namespace

void Path::ensureMoveTo() {
  if (!hasMove_) {
    verbs_.push_back(Verb::Move);
    points_.push_back({0, 0});
    lastPt_ = {0, 0};
    hasMove_ = true;
    dirtyBounds();
  }
}

Path &Path::moveTo(f32 x, f32 y) {
  verbs_.push_back(Verb::Move);
  points_.push_back({x, y});
  lastPt_ = {x, y};
  hasMove_ = true;
  dirtyBounds();
  return *this;
}

Path &Path::lineTo(f32 x, f32 y) {
  ensureMoveTo();
  verbs_.push_back(Verb::Line);
  points_.push_back({x, y});
  lastPt_ = {x, y};
  dirtyBounds();
  return *this;
}

Path &Path::quadTo(f32 x1, f32 y1, f32 x2, f32 y2) {
  ensureMoveTo();
  verbs_.push_back(Verb::Quad);
  points_.push_back({x1, y1});
  points_.push_back({x2, y2});
  lastPt_ = {x2, y2};
  dirtyBounds();
  return *this;
}

Path &Path::cubicTo(f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3) {
  ensureMoveTo();
  verbs_.push_back(Verb::Cubic);
  points_.push_back({x1, y1});
  points_.push_back({x2, y2});
  points_.push_back({x3, y3});
  lastPt_ = {x3, y3};
  dirtyBounds();
  return *this;
}

Path &Path::conicTo(f32 x1, f32 y1, f32 x2, f32 y2, f32 weight) {
  ensureMoveTo();
  verbs_.push_back(Verb::Conic);
  points_.push_back({x1, y1});
  points_.push_back({x2, y2});
  conicWeights_.push_back(weight);
  lastPt_ = {x2, y2};
  dirtyBounds();
  return *this;
}

Path &Path::close() {
  if (!verbs_.empty()) {
    verbs_.push_back(Verb::Close);
    hasMove_ = false;
  }
  return *this;
}

Path &Path::rLineTo(f32 dx, f32 dy) {
  return lineTo(lastPt_.x + dx, lastPt_.y + dy);
}

Path &Path::rQuadTo(f32 dx1, f32 dy1, f32 dx2, f32 dy2) {
  return quadTo(lastPt_.x + dx1, lastPt_.y + dy1, lastPt_.x + dx2,
                lastPt_.y + dy2);
}

Path &Path::rCubicTo(f32 dx1, f32 dy1, f32 dx2, f32 dy2, f32 dx3, f32 dy3) {
  return cubicTo(lastPt_.x + dx1, lastPt_.y + dy1, lastPt_.x + dx2,
                 lastPt_.y + dy2, lastPt_.x + dx3, lastPt_.y + dy3);
}

// --- Convenience builders ---

Path &Path::addRect(Rect r, PathDirection dir) {
  if (dir == PathDirection::CW) {
    moveTo(r.x, r.y);
    lineTo(r.x + r.w, r.y);
    lineTo(r.x + r.w, r.y + r.h);
    lineTo(r.x, r.y + r.h);
  } else {
    moveTo(r.x, r.y);
    lineTo(r.x, r.y + r.h);
    lineTo(r.x + r.w, r.y + r.h);
    lineTo(r.x + r.w, r.y);
  }
  close();
  return *this;
}

static void addArcPoints(Path &path, f32 cx, f32 cy, f32 rx, f32 ry,
                         f32 startRad, f32 sweepRad, bool forceMoveTo) {
  // Approximate arc with cubic Beziers (max 90 degrees per segment)
  i32 numSegments = std::max(
      1, static_cast<i32>(std::ceil(std::abs(sweepRad) / (kPI * 0.5f))));
  f32 segSweep = sweepRad / numSegments;
  f32 t = std::tan(segSweep * 0.5f);
  f32 alpha =
      std::sin(segSweep) * (std::sqrt(4.0f + 3.0f * t * t) - 1.0f) / 3.0f;

  f32 angle = startRad;
  f32 cosA = std::cos(angle), sinA = std::sin(angle);
  f32 px = cx + rx * cosA, py = cy + ry * sinA;

  if (forceMoveTo || path.isEmpty()) {
    path.moveTo(px, py);
  } else {
    path.lineTo(px, py);
  }

  for (i32 i = 0; i < numSegments; ++i) {
    f32 nextAngle = angle + segSweep;
    f32 cosB = std::cos(nextAngle), sinB = std::sin(nextAngle);
    f32 qx = cx + rx * cosB, qy = cy + ry * sinB;

    f32 dx0 = -rx * sinA, dy0 = ry * cosA;
    f32 dx1 = -rx * sinB, dy1 = ry * cosB;

    path.cubicTo(px + alpha * dx0, py + alpha * dy0, qx - alpha * dx1,
                 qy - alpha * dy1, qx, qy);

    px = qx;
    py = qy;
    angle = nextAngle;
    cosA = cosB;
    sinA = sinB;
  }
}

Path &Path::addOval(Rect bounds, PathDirection dir) {
  f32 cx = bounds.x + bounds.w * 0.5f;
  f32 cy = bounds.y + bounds.h * 0.5f;
  f32 rx = bounds.w * 0.5f;
  f32 ry = bounds.h * 0.5f;
  f32 sweep = (dir == PathDirection::CW) ? 2.0f * kPI : -2.0f * kPI;
  addArcPoints(*this, cx, cy, rx, ry, 0, sweep, true);
  close();
  return *this;
}

Path &Path::addCircle(f32 cx, f32 cy, f32 radius, PathDirection dir) {
  return addOval({cx - radius, cy - radius, radius * 2, radius * 2}, dir);
}

Path &Path::addRoundRect(Rect r, f32 rx, f32 ry, PathDirection dir) {
  rx = std::min(rx, r.w * 0.5f);
  ry = std::min(ry, r.h * 0.5f);
  if (rx <= 0 || ry <= 0)
    return addRect(r, dir);

  // Magic number for cubic approximation of quarter circle: (4/3)(sqrt(2)-1)
  constexpr f32 k = 0.5522847498f;
  f32 kx = rx * k, ky = ry * k;
  f32 x = r.x, y = r.y, w = r.w, h = r.h;

  if (dir == PathDirection::CW) {
    moveTo(x + rx, y);
    lineTo(x + w - rx, y);
    cubicTo(x + w - rx + kx, y, x + w, y + ry - ky, x + w, y + ry);
    lineTo(x + w, y + h - ry);
    cubicTo(x + w, y + h - ry + ky, x + w - rx + kx, y + h, x + w - rx, y + h);
    lineTo(x + rx, y + h);
    cubicTo(x + rx - kx, y + h, x, y + h - ry + ky, x, y + h - ry);
    lineTo(x, y + ry);
    cubicTo(x, y + ry - ky, x + rx - kx, y, x + rx, y);
  } else {
    moveTo(x + rx, y);
    cubicTo(x + rx - kx, y, x, y + ry - ky, x, y + ry);
    lineTo(x, y + h - ry);
    cubicTo(x, y + h - ry + ky, x + rx - kx, y + h, x + rx, y + h);
    lineTo(x + w - rx, y + h);
    cubicTo(x + w - rx + kx, y + h, x + w, y + h - ry + ky, x + w, y + h - ry);
    lineTo(x + w, y + ry);
    cubicTo(x + w, y + ry - ky, x + w - rx + kx, y, x + w - rx, y);
  }
  close();
  return *this;
}

Path &Path::addArc(Rect oval, f32 startAngleDeg, f32 sweepAngleDeg) {
  if (sweepAngleDeg == 0)
    return *this;
  f32 cx = oval.x + oval.w * 0.5f;
  f32 cy = oval.y + oval.h * 0.5f;
  f32 rx = oval.w * 0.5f, ry = oval.h * 0.5f;
  f32 startRad = startAngleDeg * kPI / 180.0f;
  f32 sweepRad = sweepAngleDeg * kPI / 180.0f;
  addArcPoints(*this, cx, cy, rx, ry, startRad, sweepRad, true);
  return *this;
}

Path &Path::addPoly(const Point *pts, i32 count, bool doClose) {
  if (count <= 0)
    return *this;
  moveTo(pts[0].x, pts[0].y);
  for (i32 i = 1; i < count; ++i)
    lineTo(pts[i].x, pts[i].y);
  if (doClose)
    close();
  return *this;
}

Path &Path::addPath(const Path &other) {
  verbs_.insert(verbs_.end(), other.verbs_.begin(), other.verbs_.end());
  points_.insert(points_.end(), other.points_.begin(), other.points_.end());
  conicWeights_.insert(conicWeights_.end(), other.conicWeights_.begin(),
                       other.conicWeights_.end());
  if (!other.verbs_.empty()) {
    lastPt_ = other.lastPt_;
    hasMove_ = other.hasMove_;
  }
  dirtyBounds();
  return *this;
}

// --- Accessors ---

Rect Path::bounds() const {
  if (!boundsDirty_)
    return cachedBounds_;
  auto contours = flattenPath(*this);
  if (contours.empty()) {
    cachedBounds_ = {0, 0, 0, 0};
    boundsDirty_ = false;
    return cachedBounds_;
  }

  f32 minX = std::numeric_limits<f32>::max();
  f32 minY = std::numeric_limits<f32>::max();
  f32 maxX = std::numeric_limits<f32>::lowest();
  f32 maxY = std::numeric_limits<f32>::lowest();
  for (const auto &contour : contours) {
    for (const auto &p : contour.points) {
      minX = std::min(minX, p.x);
      minY = std::min(minY, p.y);
      maxX = std::max(maxX, p.x);
      maxY = std::max(maxY, p.y);
    }
  }
  cachedBounds_ = {minX, minY, maxX - minX, maxY - minY};
  boundsDirty_ = false;
  return cachedBounds_;
}

bool Path::contains(Point p) const {
  auto contours = flattenPath(*this);
  i32 winding = 0;
  i32 crossings = 0;

  for (const auto &contour : contours) {
    const auto &pts = contour.points;
    if (pts.size() < 2)
      continue;

    size_t count = pts.size();
    for (size_t i = 0; i < count; ++i) {
      Point a = pts[i];
      Point b = pts[(i + 1) % count];
      if (std::abs(a.y - b.y) < 1e-6f)
        continue;
      bool upward = a.y <= p.y && b.y > p.y;
      bool downward = b.y <= p.y && a.y > p.y;
      if (!upward && !downward)
        continue;

      f32 t = (p.y - a.y) / (b.y - a.y);
      f32 x = a.x + t * (b.x - a.x);
      if (x <= p.x)
        continue;

      ++crossings;
      winding += upward ? 1 : -1;
    }
  }

  return fillRule_ == FillRule::EvenOdd ? ((crossings & 1) != 0)
                                        : (winding != 0);
}

bool Path::getDirection(PathDirection *direction) const {
  auto contours = flattenPath(*this);
  f32 area = 0.0f;
  for (const auto &contour : contours) {
    area += signedArea(contour);
  }
  if (std::abs(area) < 1e-6f)
    return false;
  if (direction)
    *direction = area >= 0 ? PathDirection::CW : PathDirection::CCW;
  return true;
}

PathDirection Path::direction() const {
  PathDirection dir = PathDirection::CW;
  getDirection(&dir);
  return dir;
}

bool Path::isConvex() const {
  auto contours = flattenPath(*this);
  i32 usableContours = 0;
  const FlatContour *contour = nullptr;
  for (const auto &c : contours) {
    if (c.points.size() >= 3) {
      ++usableContours;
      contour = &c;
    }
  }
  if (usableContours == 0)
    return true;
  if (usableContours > 1)
    return false;

  const auto &pts = contour->points;
  bool gotPositive = false;
  bool gotNegative = false;
  for (size_t i = 0; i < pts.size(); ++i) {
    const Point &a = pts[i];
    const Point &b = pts[(i + 1) % pts.size()];
    const Point &c = pts[(i + 2) % pts.size()];
    f32 dx1 = b.x - a.x;
    f32 dy1 = b.y - a.y;
    f32 dx2 = c.x - b.x;
    f32 dy2 = c.y - b.y;
    f32 cross = dx1 * dy2 - dy1 * dx2;
    if (cross > 1e-5f)
      gotPositive = true;
    if (cross < -1e-5f)
      gotNegative = true;
    if (gotPositive && gotNegative)
      return false;
  }
  return true;
}

// --- Mutation ---

void Path::reset() {
  verbs_.clear();
  points_.clear();
  conicWeights_.clear();
  fillRule_ = FillRule::Winding;
  lastPt_ = {0, 0};
  hasMove_ = false;
  dirtyBounds();
}

void Path::transform(const Matrix &m) {
  for (auto &p : points_) {
    p = m.mapPoint(p);
  }
  lastPt_ = m.mapPoint(lastPt_);
  dirtyBounds();
}

Path Path::transformed(const Matrix &m) const {
  Path result = *this;
  result.transform(m);
  return result;
}

void Path::offset(f32 dx, f32 dy) { transform(Matrix::Translate(dx, dy)); }

} // namespace ink
