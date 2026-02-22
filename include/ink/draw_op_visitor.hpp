#pragma once

/**
 * @file draw_op_visitor.hpp
 * @brief Visitor interface for traversing recorded draw operations.
 */

#include "ink/types.hpp"
#include <cstdint>
#include <memory>

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
    virtual void visitFillRect(Rect rect, Color color) = 0;

    /// @brief Visit a stroked rectangle command.
    /// @param rect The rectangle to stroke.
    /// @param color Stroke color.
    /// @param width Stroke line width.
    virtual void visitStrokeRect(Rect rect, Color color, f32 width) = 0;

    /// @brief Visit a line segment command.
    /// @param p1 Start point.
    /// @param p2 End point.
    /// @param color Line color.
    /// @param width Line width.
    virtual void visitLine(Point p1, Point p2, Color color, f32 width) = 0;

    /// @brief Visit a polyline command.
    /// @param points Array of vertices.
    /// @param count Number of points.
    /// @param color Line color.
    /// @param width Line width.
    virtual void visitPolyline(const Point* points, i32 count, Color color, f32 width) = 0;

    /// @brief Visit a text drawing command.
    /// @param pos Position of the text baseline.
    /// @param text UTF-8 text string.
    /// @param len Length of the text in bytes.
    /// @param color Text color.
    virtual void visitText(Point pos, const char* text, u32 len, Color color) = 0;

    /// @brief Visit an image drawing command.
    /// @param image Pointer to the image to draw.
    /// @param x X position to draw at.
    /// @param y Y position to draw at.
    virtual void visitDrawImage(const Image* image, f32 x, f32 y) = 0;

    /// @brief Visit a clip rectangle command.
    /// @param rect The clipping rectangle.
    virtual void visitSetClip(Rect rect) = 0;

    /// @brief Visit a clear-clip command (removes current clip).
    virtual void visitClearClip() = 0;
};

} // namespace ink
