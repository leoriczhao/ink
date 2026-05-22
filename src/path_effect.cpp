#include "ink/path_effect.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ink {

namespace {

static constexpr f32 kFlatTolerance = 0.25f;

struct FlatContour {
  std::vector<Point> points;
  bool closed = false;
};

static f32 distToLineSq(Point p, Point a, Point b) {
  f32 dx = b.x - a.x;
  f32 dy = b.y - a.y;
  f32 lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-12f)
    return 0.0f;
  f32 cross = (p.x - a.x) * dy - (p.y - a.y) * dx;
  return (cross * cross) / lenSq;
}

static void flattenQuadInto(std::vector<Point> &out, Point p0, Point p1,
                            Point p2) {
  if (distToLineSq(p1, p0, p2) <= kFlatTolerance * kFlatTolerance) {
    out.push_back(p2);
    return;
  }
  Point q0 = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
  Point q1 = {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
  Point mid = {(q0.x + q1.x) * 0.5f, (q0.y + q1.y) * 0.5f};
  flattenQuadInto(out, p0, q0, mid);
  flattenQuadInto(out, mid, q1, p2);
}

static void flattenCubicInto(std::vector<Point> &out, Point p0, Point p1,
                             Point p2, Point p3) {
  if (std::max(distToLineSq(p1, p0, p3), distToLineSq(p2, p0, p3)) <=
      kFlatTolerance * kFlatTolerance) {
    out.push_back(p3);
    return;
  }
  Point ab = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
  Point bc = {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
  Point cd = {(p2.x + p3.x) * 0.5f, (p2.y + p3.y) * 0.5f};
  Point abc = {(ab.x + bc.x) * 0.5f, (ab.y + bc.y) * 0.5f};
  Point bcd = {(bc.x + cd.x) * 0.5f, (bc.y + cd.y) * 0.5f};
  Point mid = {(abc.x + bcd.x) * 0.5f, (abc.y + bcd.y) * 0.5f};
  flattenCubicInto(out, p0, ab, abc, mid);
  flattenCubicInto(out, mid, bcd, cd, p3);
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
                             Point p2, f32 w) {
  i32 steps =
      std::max(4, static_cast<i32>(std::ceil(
                      std::sqrt(std::max(distToLineSq(p1, p0, p2), 1.0f)) /
                      kFlatTolerance)));
  steps = std::min(steps, 64);
  for (i32 i = 1; i <= steps; ++i) {
    out.push_back(evalConic(p0, p1, p2, w, static_cast<f32>(i) / steps));
  }
}

static std::vector<FlatContour> flattenPath(const Path &path) {
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
    case Path::Verb::Line:
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      cur = pts[ptIdx++];
      contours.back().points.push_back(cur);
      break;
    case Path::Verb::Quad: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      Point cp = pts[ptIdx++];
      Point end = pts[ptIdx++];
      flattenQuadInto(contours.back().points, cur, cp, end);
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
      flattenCubicInto(contours.back().points, cur, c1, c2, end);
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
      flattenConicInto(contours.back().points, cur, cp, end,
                       weights[weightIdx++]);
      cur = end;
      break;
    }
    case Path::Verb::Close:
      if (!contours.empty())
        contours.back().closed = true;
      break;
    }
  }
  return contours;
}

class DashPathEffect final : public PathEffect {
public:
  DashPathEffect(std::vector<f32> intervals, f32 phase)
      : intervals_(std::move(intervals)), phase_(phase) {}

  Path filterPath(const Path &src) const override {
    Path out;
    out.setFillRule(src.fillRule());
    for (const auto &contour : flattenPath(src)) {
      dashContour(contour, out);
    }
    return out;
  }

private:
  std::vector<f32> intervals_;
  f32 phase_ = 0.0f;

  struct State {
    i32 index = 0;
    f32 remaining = 0.0f;
    bool on = true;
  };

  f32 totalLength() const {
    f32 total = 0.0f;
    for (f32 v : intervals_)
      total += v;
    return total;
  }

  State initialState() const {
    f32 total = totalLength();
    f32 phase = std::fmod(phase_, total);
    if (phase < 0)
      phase += total;
    State state;
    state.remaining = intervals_[0];
    while (phase > 0 && phase >= state.remaining) {
      phase -= state.remaining;
      state.index = (state.index + 1) % static_cast<i32>(intervals_.size());
      state.remaining = intervals_[state.index];
    }
    state.remaining -= phase;
    state.on = (state.index & 1) == 0;
    return state;
  }

  void advance(State &state) const {
    state.index = (state.index + 1) % static_cast<i32>(intervals_.size());
    state.remaining = intervals_[state.index];
    state.on = (state.index & 1) == 0;
  }

  static Point lerp(Point a, Point b, f32 t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
  }

  void dashContour(const FlatContour &contour, Path &out) const {
    const auto &pts = contour.points;
    if (pts.size() < 2)
      return;
    State state = initialState();
    size_t segmentCount = contour.closed ? pts.size() : pts.size() - 1;

    for (size_t i = 0; i < segmentCount; ++i) {
      Point a = pts[i];
      Point b = pts[(i + 1) % pts.size()];
      f32 dx = b.x - a.x;
      f32 dy = b.y - a.y;
      f32 len = std::sqrt(dx * dx + dy * dy);
      if (len < 1e-6f)
        continue;

      f32 consumed = 0.0f;
      while (consumed < len) {
        f32 take = std::min(state.remaining, len - consumed);
        if (state.on && take > 1e-6f) {
          Point p0 = lerp(a, b, consumed / len);
          Point p1 = lerp(a, b, (consumed + take) / len);
          out.moveTo(p0);
          out.lineTo(p1);
        }
        consumed += take;
        state.remaining -= take;
        if (state.remaining <= 1e-6f)
          advance(state);
      }
    }
  }
};

} // namespace

namespace PathEffects {

std::shared_ptr<PathEffect> MakeDash(const f32 *intervals, i32 count,
                                     f32 phase) {
  if (!intervals || count <= 0)
    return nullptr;
  std::vector<f32> values;
  values.reserve((count & 1) ? count * 2 : count);
  f32 total = 0.0f;
  for (i32 i = 0; i < count; ++i) {
    if (intervals[i] <= 0.0f)
      return nullptr;
    values.push_back(intervals[i]);
    total += intervals[i];
  }
  if (total <= 0.0f)
    return nullptr;
  if (count & 1) {
    for (i32 i = 0; i < count; ++i)
      values.push_back(intervals[i]);
  }
  return std::make_shared<DashPathEffect>(std::move(values), phase);
}

} // namespace PathEffects

} // namespace ink
