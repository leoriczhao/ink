#pragma once

#include "ink/types.hpp"
#include <cstdint>
#include <memory>

namespace ink {

struct CompactDrawOp;
struct DrawOpArena;
class Image;

// Visitor interface for traversing CompactDrawOp operations.
class DrawOpVisitor {
public:
    virtual ~DrawOpVisitor() = default;

    virtual void visitFillRect(Rect rect, Color color) = 0;
    virtual void visitStrokeRect(Rect rect, Color color, f32 width) = 0;
    virtual void visitLine(Point p1, Point p2, Color color, f32 width) = 0;
    virtual void visitPolyline(const Point* points, i32 count, Color color, f32 width) = 0;
    virtual void visitText(Point pos, const char* text, u32 len, Color color) = 0;
    virtual void visitDrawImage(const Image* image, f32 x, f32 y) = 0;
    virtual void visitSetClip(Rect rect) = 0;
    virtual void visitClearClip() = 0;
};

} // namespace ink
