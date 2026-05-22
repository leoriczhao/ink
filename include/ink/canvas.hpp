#pragma once

/**
 * @file canvas.hpp
 * @brief User-facing drawing API with state management.
 */

#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include "ink/path.hpp"
#include "ink/types.hpp"
#include <memory>
#include <string_view>
#include <vector>

namespace ink {

class Device;
class Image;

/// @brief User-facing drawing API.
class Canvas {
public:
  explicit Canvas(Device *device);

  // --- Simple color-based drawing ---
  void fillRect(Rect r, Color c);
  void strokeRect(Rect r, Color c, f32 width = 1.0f);
  void drawLine(Point p1, Point p2, Color c, f32 width = 1.0f);
  void drawPolyline(const Point *pts, i32 count, Color c, f32 width = 1.0f);
  void drawText(Point p, std::string_view text, Color c);
  void drawImage(std::shared_ptr<Image> image, f32 x, f32 y);
  void fillCircle(f32 cx, f32 cy, f32 radius, Color c);
  void strokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width = 1.0f);
  void fillRoundRect(Rect r, f32 rx, f32 ry, Color c);
  void strokeRoundRect(Rect r, f32 rx, f32 ry, Color c, f32 width = 1.0f);

  /// @name Paint-based drawing (advanced)
  /// @{
  void draw(Rect r, const Paint &paint);
  void draw(Point p1, Point p2, const Paint &paint);
  /// @}

  /// @name Path drawing
  /// @{
  /// @brief Draw a path with the given paint.
  void drawPath(const Path &path, const Paint &paint);
  /// @}

  /// @name Image rect drawing
  /// @{
  /// @brief Draw an image mapping src rect to dst rect.
  void drawImageRect(std::shared_ptr<Image> image, Rect src, Rect dst);

  /// @brief Draw an image into the dst rect (src = full image).
  void drawImageRect(std::shared_ptr<Image> image, Rect dst);
  /// @}

  // --- State management ---
  void save();
  void restore();
  void clipRect(Rect r);
  /// @brief Intersect the current clip with a filled path.
  void clipPath(const Path &path);

  /// @brief Translate the current transform.
  void translate(f32 dx, f32 dy);
  void rotate(f32 radians);
  void scale(f32 sx, f32 sy);
  void concat(const Matrix &m);
  void setMatrix(const Matrix &m);
  Matrix getMatrix() const;

private:
  struct State {
    bool hasClip = false;
    Rect clip = {};
    std::vector<Path> pathClips;
    Matrix transform;
  };

  Device *device_ = nullptr;
  std::vector<State> stack_;
  State current_;

  void applyClip();
  void applyTransform();
};

} // namespace ink
