#pragma once

/**
 * @file device.hpp
 * @brief Recording device that converts Canvas commands into low-level draw
 * operations.
 */

#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include "ink/recording.hpp"
#include "ink/types.hpp"
#include <memory>

namespace ink {

class GlyphCache;
class Image;
class Path;

/// @brief The single recording device.
class Device {
public:
  Device() = default;
  ~Device() = default;

  void beginFrame();
  void endFrame();

  /// @name Drawing commands (Paint-based)
  /// @{
  void fillRect(Rect r, const Paint &paint);
  void strokeRect(Rect r, const Paint &paint);
  void drawLine(Point p1, Point p2, const Paint &paint);
  void drawPolyline(const Point *pts, i32 count, const Paint &paint);
  void drawText(Point p, std::string_view text, const Paint &paint);
  void drawImage(std::shared_ptr<Image> image, f32 x, f32 y);
  void fillCircle(f32 cx, f32 cy, f32 radius, const Paint &paint);
  void strokeCircle(f32 cx, f32 cy, f32 radius, const Paint &paint);
  void fillRoundRect(Rect r, f32 rx, f32 ry, const Paint &paint);
  void strokeRoundRect(Rect r, f32 rx, f32 ry, const Paint &paint);
  /// @}

  /// @name Path drawing
  /// @{
  void drawPath(const Path &path, const Paint &paint);
  /// @}

  /// @name Image rect drawing
  /// @{
  void drawImageRect(std::shared_ptr<Image> image, Rect src, Rect dst,
                     const Paint &paint);
  /// @}

  /// @brief Set the current clip rectangle.
  void setClipRect(Rect r);
  void setClipPath(const Path &path);
  void resetClip();
  void setTransform(const Matrix &m);
  void clearTransform();

  std::unique_ptr<Recording> finishRecording();

private:
  Recorder recorder_;
  std::unique_ptr<Recording> recording_;
};

} // namespace ink
