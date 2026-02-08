#include "ink/recording.hpp"
#include "ink/draw_op_visitor.hpp"

namespace ink {

DrawOpArena::DrawOpArena(size_t initialCapacity) {
    data_.reserve(initialCapacity);
}

u32 DrawOpArena::allocate(size_t bytes) {
    u32 offset = static_cast<u32>(data_.size());
    data_.resize(data_.size() + bytes);
    return offset;
}

u32 DrawOpArena::storeString(std::string_view str) {
    u32 offset = static_cast<u32>(data_.size());
    size_t len = str.size();
    data_.insert(data_.end(), reinterpret_cast<const u8*>(str.data()), 
                 reinterpret_cast<const u8*>(str.data()) + len);
    data_.push_back(0);
    return offset;
}

u32 DrawOpArena::storePoints(const Point* pts, i32 count) {
    // Align to Point's alignment (4 bytes for f32 members)
    constexpr size_t align = alignof(Point);
    size_t cur = data_.size();
    size_t aligned = (cur + align - 1) & ~(align - 1);
    if (aligned > cur) {
        data_.resize(aligned, 0);
    }
    u32 offset = static_cast<u32>(data_.size());
    size_t bytes = count * sizeof(Point);
    data_.insert(data_.end(), reinterpret_cast<const u8*>(pts),
                 reinterpret_cast<const u8*>(pts) + bytes);
    return offset;
}

const char* DrawOpArena::getString(u32 offset) const {
    return reinterpret_cast<const char*>(data_.data() + offset);
}

const Point* DrawOpArena::getPoints(u32 offset) const {
    return reinterpret_cast<const Point*>(data_.data() + offset);
}

void DrawOpArena::reset() {
    data_.clear();
}

Recording::Recording(std::vector<CompactDrawOp> ops, DrawOpArena arena)
    : ops_(std::move(ops)), arena_(std::move(arena)) {}

void Recording::accept(DrawOpVisitor& visitor) const {
    for (const auto& op : ops_) {
        switch (op.type) {
            case DrawOp::Type::FillRect:
                visitor.visitFillRect(op.data.fill.rect, op.color);
                break;
            case DrawOp::Type::StrokeRect:
                visitor.visitStrokeRect(op.data.stroke.rect, op.color, op.width);
                break;
            case DrawOp::Type::Line:
                visitor.visitLine(op.data.line.p1, op.data.line.p2, op.color, op.width);
                break;
            case DrawOp::Type::Polyline:
                if (op.data.polyline.count > 0) {
                    const Point* points = arena_.getPoints(op.data.polyline.offset);
                    visitor.visitPolyline(points, op.data.polyline.count, op.color, op.width);
                }
                break;
            case DrawOp::Type::Text:
                visitor.visitText(op.data.text.pos, arena_.getString(op.data.text.offset),
                                op.data.text.len, op.color);
                break;
            case DrawOp::Type::SetClip:
                visitor.visitSetClip(op.data.clip.rect);
                break;
            case DrawOp::Type::ClearClip:
                visitor.visitClearClip();
                break;
        }
    }
}

void Recorder::reset() {
    ops_.clear();
    arena_.reset();
}

void Recorder::fillRect(Rect r, Color c) {
    CompactDrawOp op;
    op.type = DrawOp::Type::FillRect;
    op.color = c;
    op.width = 1.0f;
    op.data.fill.rect = r;
    ops_.push_back(op);
}

void Recorder::strokeRect(Rect r, Color c, f32 width) {
    CompactDrawOp op;
    op.type = DrawOp::Type::StrokeRect;
    op.color = c;
    op.width = width;
    op.data.stroke.rect = r;
    ops_.push_back(op);
}

void Recorder::drawLine(Point p1, Point p2, Color c, f32 width) {
    CompactDrawOp op;
    op.type = DrawOp::Type::Line;
    op.color = c;
    op.width = width;
    op.data.line.p1 = p1;
    op.data.line.p2 = p2;
    ops_.push_back(op);
}

void Recorder::drawPolyline(const Point* pts, i32 count, Color c, f32 width) {
    if (count < 2) return;
    CompactDrawOp op;
    op.type = DrawOp::Type::Polyline;
    op.color = c;
    op.width = width;
    op.data.polyline.offset = arena_.storePoints(pts, count);
    op.data.polyline.count = static_cast<u32>(count);
    ops_.push_back(op);
}

void Recorder::drawText(Point p, std::string_view text, Color c) {
    CompactDrawOp op;
    op.type = DrawOp::Type::Text;
    op.color = c;
    op.width = 1.0f;
    op.data.text.pos = p;
    op.data.text.offset = arena_.storeString(text);
    op.data.text.len = static_cast<u32>(text.size());
    ops_.push_back(op);
}

void Recorder::setClip(Rect r) {
    CompactDrawOp op;
    op.type = DrawOp::Type::SetClip;
    op.color = {};
    op.width = 1.0f;
    op.data.clip.rect = r;
    ops_.push_back(op);
}

void Recorder::clearClip() {
    CompactDrawOp op;
    op.type = DrawOp::Type::ClearClip;
    op.color = {};
    op.width = 1.0f;
    ops_.push_back(op);
}

std::unique_ptr<Recording> Recorder::finish() {
    auto recording = std::make_unique<Recording>(std::move(ops_), std::move(arena_));
    ops_.clear();
    arena_.reset();
    return recording;
}

}
