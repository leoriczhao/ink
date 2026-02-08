#pragma once

#include "ink/types.hpp"
#include <cstdint>

namespace ink {

struct CompactDrawOp;
struct DrawOpArena;

// Visitor interface for traversing CompactDrawOp operations.
// Each visit method receives the operation data and arena for accessing
// variable-length data (strings, point arrays).
class DrawOpVisitor {
public:
    virtual ~DrawOpVisitor() = default;

    // Visit a FillRect operation
    virtual void visitFillRect(Rect rect, Color color) = 0;

    // Visit a StrokeRect operation
    virtual void visitStrokeRect(Rect rect, Color color, f32 width) = 0;

    // Visit a Line operation
    virtual void visitLine(Point p1, Point p2, Color color, f32 width) = 0;

    // Visit a Polyline operation
    virtual void visitPolyline(const Point* points, i32 count, Color color, f32 width) = 0;

    // Visit a Text operation
    virtual void visitText(Point pos, const char* text, u32 len, Color color) = 0;

    // Visit a SetClip operation
    virtual void visitSetClip(Rect rect) = 0;

    // Visit a ClearClip operation
    virtual void visitClearClip() = 0;
};

} // namespace ink
