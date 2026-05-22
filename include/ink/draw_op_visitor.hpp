#pragma once

/**
 * @file draw_op_visitor.hpp
 * @brief Visitor interface for traversing recorded draw operations.
 */

#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include "ink/types.hpp"

namespace ink {

struct CompactDrawOp;
class DrawOpArena;
class Image;
class Path;
class Shader;
class ColorFilter;

/// @brief Visitor interface for traversing recorded draw operations.
class DrawOpVisitor {
public:
  virtual ~DrawOpVisitor() = default;

  virtual void visitFillRect(Rect rect, Color color, BlendMode blend,
                             u8 opacity) = 0;

  virtual void visitStrokeRect(Rect rect, Color color, f32 width,
                               BlendMode blend, u8 opacity) = 0;

  virtual void visitLine(Point p1, Point p2, Color color, f32 width,
                         BlendMode blend, u8 opacity) = 0;

  virtual void visitPolyline(const Point *points, i32 count, Color color,
                             f32 width, BlendMode blend, u8 opacity) = 0;

  virtual void visitText(Point pos, const char *text, u32 len, Color color,
                         BlendMode blend, u8 opacity) = 0;

  virtual void visitDrawImage(const Image *image, f32 x, f32 y, BlendMode blend,
                              u8 opacity) = 0;

  virtual void visitSetClip(Rect rect) = 0;
  virtual void visitSetClipPath(const Path &path, u8 fillRule,
                                bool antiAlias) = 0;
  virtual void visitClearClip() = 0;
  virtual void visitSetTransform(const Matrix &m) = 0;
  virtual void visitClearTransform() = 0;

  virtual void visitFillCircle(f32 cx, f32 cy, f32 radius, Color color,
                               BlendMode blend, u8 opacity) = 0;

  virtual void visitStrokeCircle(f32 cx, f32 cy, f32 radius, Color color,
                                 f32 width, BlendMode blend, u8 opacity) = 0;

  virtual void visitFillRoundRect(Rect rect, f32 rx, f32 ry, Color color,
                                  BlendMode blend, u8 opacity) = 0;

  virtual void visitStrokeRoundRect(Rect rect, f32 rx, f32 ry, Color color,
                                    f32 width, BlendMode blend, u8 opacity) = 0;

  /// @brief Visit a filled path command.
  virtual void visitFillPath(const Path &path, Color color, BlendMode blend,
                             u8 opacity, u8 fillRule, bool antiAlias,
                             const Shader *shader,
                             const ColorFilter *colorFilter) = 0;

  /// @brief Visit a stroked path command.
  virtual void visitStrokePath(const Path &path, Color color, f32 width,
                               BlendMode blend, u8 opacity, u8 cap, u8 join,
                               f32 miterLimit, bool antiAlias,
                               const Shader *shader,
                               const ColorFilter *colorFilter) = 0;

  /// @brief Visit an image-rect drawing command.
  virtual void visitDrawImageRect(const Image *image, Rect src, Rect dst,
                                  BlendMode blend, u8 opacity,
                                  FilterQuality quality) = 0;
};

} // namespace ink
