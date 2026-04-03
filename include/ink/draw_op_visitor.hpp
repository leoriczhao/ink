#pragma once

/**
 * @file draw_op_visitor.hpp
 * @brief Visitor interface for traversing recorded draw operations.
 */

#include "ink/types.hpp"
#include "ink/matrix.hpp"
#include "ink/paint.hpp"

namespace ink {

struct CompactDrawOp;
class DrawOpArena;
class Image;

/// @brief Visitor interface for traversing recorded draw operations.
///
/// Implement this interface to process drawing commands dispatched by
/// Recording::accept() or Recording::dispatch().
class DrawOpVisitor {
public:
    virtual ~DrawOpVisitor() = default;

    /// @brief Visit a filled rectangle command.
    /// @param rect The rectangle to fill.
    /// @param color Fill color.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitFillRect(Rect rect, Color color, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit a stroked rectangle command.
    /// @param rect The rectangle to stroke.
    /// @param color Stroke color.
    /// @param width Stroke line width.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitStrokeRect(Rect rect, Color color, f32 width, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit a line segment command.
    /// @param p1 Start point.
    /// @param p2 End point.
    /// @param color Line color.
    /// @param width Line width.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitLine(Point p1, Point p2, Color color, f32 width, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit a polyline command.
    /// @param points Array of vertices.
    /// @param count Number of points.
    /// @param color Line color.
    /// @param width Line width.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitPolyline(const Point* points, i32 count, Color color, f32 width, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit a text drawing command.
    /// @param pos Position of the text baseline.
    /// @param text UTF-8 text string.
    /// @param len Length of the text in bytes.
    /// @param color Text color.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitText(Point pos, const char* text, u32 len, Color color, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit an image drawing command.
    /// @param image Pointer to the image to draw.
    /// @param x X position to draw at.
    /// @param y Y position to draw at.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitDrawImage(const Image* image, f32 x, f32 y, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit a clip rectangle command.
    /// @param rect The clipping rectangle.
    virtual void visitSetClip(Rect rect) = 0;

    /// @brief Visit a clear-clip command (removes current clip).
    virtual void visitClearClip() = 0;

    /// @brief Visit a set-transform command.
    /// @param m The transformation matrix.
    virtual void visitSetTransform(const Matrix& m) = 0;

    /// @brief Visit a clear-transform command (resets to identity).
    virtual void visitClearTransform() = 0;

    /// @brief Visit a filled circle command.
    /// @param cx Center X coordinate.
    /// @param cy Center Y coordinate.
    /// @param radius Circle radius.
    /// @param color Fill color.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitFillCircle(f32 cx, f32 cy, f32 radius, Color color, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit a stroked circle command.
    /// @param cx Center X coordinate.
    /// @param cy Center Y coordinate.
    /// @param radius Circle radius.
    /// @param color Stroke color.
    /// @param width Stroke line width.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitStrokeCircle(f32 cx, f32 cy, f32 radius, Color color, f32 width, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit a filled rounded rectangle command.
    /// @param rect The rectangle.
    /// @param rx Corner radius in X.
    /// @param ry Corner radius in Y.
    /// @param color Fill color.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitFillRoundRect(Rect rect, f32 rx, f32 ry, Color color, BlendMode blend, u8 opacity) = 0;

    /// @brief Visit a stroked rounded rectangle command.
    /// @param rect The rectangle.
    /// @param rx Corner radius in X.
    /// @param ry Corner radius in Y.
    /// @param color Stroke color.
    /// @param width Stroke line width.
    /// @param blend Blend mode.
    /// @param opacity Opacity 0-255.
    virtual void visitStrokeRoundRect(Rect rect, f32 rx, f32 ry, Color color, f32 width, BlendMode blend, u8 opacity) = 0;
};

} // namespace ink
