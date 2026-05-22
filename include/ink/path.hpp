#pragma once

/**
 * @file path.hpp
 * @brief General-purpose 2D vector path with curves and convenience builders.
 */

#include "ink/matrix.hpp"
#include "ink/types.hpp"
#include <vector>

namespace ink {

/// @brief Fill rule for path rendering.
enum class FillRule : u8 {
  Winding, ///< Non-zero winding rule.
  EvenOdd, ///< Even-odd (parity) rule.
};

/// @brief Line cap style for stroked paths.
enum class LineCap : u8 {
  Butt,   ///< Flat cap at the endpoint.
  Round,  ///< Semicircular cap.
  Square, ///< Extended flat cap (half stroke width).
};

/// @brief Line join style for stroked paths.
enum class LineJoin : u8 {
  Miter, ///< Sharp corner (extended to miter limit).
  Round, ///< Rounded corner.
  Bevel, ///< Flat corner.
};

/// @brief Winding direction for shape builders.
enum class PathDirection : u8 {
  CW,  ///< Clockwise.
  CCW, ///< Counter-clockwise.
};

/// @brief General-purpose 2D vector path.
///
/// A path is a sequence of verbs (move, line, quad, cubic, conic, close) and
/// associated control points. Paths can be filled or stroked.
class Path {
public:
  enum class Verb : u8 {
    Move,  ///< Start a new contour (1 point).
    Line,  ///< Line segment (1 point — endpoint).
    Quad,  ///< Quadratic Bezier (2 points — control, end).
    Cubic, ///< Cubic Bezier (3 points — control1, control2, end).
    Conic, ///< Conic (rational quadratic, 2 points + weight).
    Close, ///< Close current contour (0 points).
  };

  Path() = default;

  /// @name Path building
  /// @{
  Path &moveTo(f32 x, f32 y);
  Path &moveTo(Point p) { return moveTo(p.x, p.y); }

  Path &lineTo(f32 x, f32 y);
  Path &lineTo(Point p) { return lineTo(p.x, p.y); }

  Path &quadTo(f32 x1, f32 y1, f32 x2, f32 y2);
  Path &quadTo(Point cp, Point end) { return quadTo(cp.x, cp.y, end.x, end.y); }

  Path &cubicTo(f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3);
  Path &cubicTo(Point c1, Point c2, Point end) {
    return cubicTo(c1.x, c1.y, c2.x, c2.y, end.x, end.y);
  }

  Path &conicTo(f32 x1, f32 y1, f32 x2, f32 y2, f32 weight);
  Path &conicTo(Point cp, Point end, f32 w) {
    return conicTo(cp.x, cp.y, end.x, end.y, w);
  }

  Path &close();
  /// @}

  /// @name Relative building commands
  /// @{
  Path &rLineTo(f32 dx, f32 dy);
  Path &rQuadTo(f32 dx1, f32 dy1, f32 dx2, f32 dy2);
  Path &rCubicTo(f32 dx1, f32 dy1, f32 dx2, f32 dy2, f32 dx3, f32 dy3);
  /// @}

  /// @name Convenience shape builders
  /// @{
  Path &addRect(Rect r, PathDirection dir = PathDirection::CW);
  Path &addOval(Rect bounds, PathDirection dir = PathDirection::CW);
  Path &addCircle(f32 cx, f32 cy, f32 radius,
                  PathDirection dir = PathDirection::CW);
  Path &addRoundRect(Rect r, f32 rx, f32 ry,
                     PathDirection dir = PathDirection::CW);
  Path &addArc(Rect oval, f32 startAngleDeg, f32 sweepAngleDeg);
  Path &addPoly(const Point *pts, i32 count, bool doClose);
  Path &addPath(const Path &other);
  /// @}

  /// @name Accessors
  /// @{
  const std::vector<Verb> &verbs() const { return verbs_; }
  const std::vector<Point> &points() const { return points_; }
  const std::vector<f32> &conicWeights() const { return conicWeights_; }

  FillRule fillRule() const { return fillRule_; }
  void setFillRule(FillRule rule) { fillRule_ = rule; }

  /// @brief Return the bounds of the flattened path geometry.
  Rect bounds() const;
  /// @brief Return true if the point is inside the path using the current fill
  /// rule.
  bool contains(Point p) const;
  bool contains(f32 x, f32 y) const { return contains({x, y}); }
  /// @brief Compute the dominant winding direction of the path.
  /// @return False for empty or degenerate paths with no measurable area.
  bool getDirection(PathDirection *direction) const;
  /// @brief Return the dominant winding direction, defaulting to CW for
  /// degenerate paths.
  PathDirection direction() const;
  bool isEmpty() const { return verbs_.empty(); }
  i32 countVerbs() const { return static_cast<i32>(verbs_.size()); }
  i32 countPoints() const { return static_cast<i32>(points_.size()); }
  bool isConvex() const;
  /// @}

  /// @name Mutation
  /// @{
  void reset();
  void transform(const Matrix &m);
  Path transformed(const Matrix &m) const;
  void offset(f32 dx, f32 dy);
  /// @}

private:
  std::vector<Verb> verbs_;
  std::vector<Point> points_;
  std::vector<f32> conicWeights_;
  FillRule fillRule_ = FillRule::Winding;
  Point lastPt_ = {0, 0};
  bool hasMove_ = false;
  mutable Rect cachedBounds_ = {};
  mutable bool boundsDirty_ = true;

  void ensureMoveTo();
  void dirtyBounds() { boundsDirty_ = true; }
};

} // namespace ink
