#include "ink/recording.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/draw_pass.hpp"
#include "ink/image.hpp"

namespace ink {

// --- DrawOpArena ---

DrawOpArena::DrawOpArena(size_t initialCapacity) {
    data_.reserve(initialCapacity);
}

u32 DrawOpArena::allocate(size_t bytes) {
    size_t current = data_.size();
    size_t padding = (4 - (current & 3)) & 3;
    data_.resize(current + padding + bytes);
    return static_cast<u32>(current + padding);
}

u32 DrawOpArena::storeString(std::string_view str) {
    u32 offset = allocate(str.size() + 1);
    std::memcpy(data_.data() + offset, str.data(), str.size());
    data_[offset + str.size()] = '\0';
    return offset;
}

u32 DrawOpArena::storePoints(const Point* pts, i32 count) {
    size_t bytes = count * sizeof(Point);
    u32 offset = allocate(bytes);
    std::memcpy(data_.data() + offset, pts, bytes);
    return offset;
}

const char* DrawOpArena::getString(u32 offset) const {
    return reinterpret_cast<const char*>(data_.data() + offset);
}

const Point* DrawOpArena::getPoints(u32 offset) const {
    return reinterpret_cast<const Point*>(data_.data() + offset);
}

u32 DrawOpArena::storeMatrix(const Matrix& m) {
    u32 offset = allocate(sizeof(Matrix));
    std::memcpy(data_.data() + offset, &m, sizeof(Matrix));
    return offset;
}

const Matrix* DrawOpArena::getMatrix(u32 offset) const {
    return reinterpret_cast<const Matrix*>(data_.data() + offset);
}

u32 DrawOpArena::storeRoundRect(Rect r, f32 rx, f32 ry) {
    // Store Rect (16 bytes) + rx (4 bytes) + ry (4 bytes) = 24 bytes
    u32 offset = allocate(sizeof(Rect) + sizeof(f32) * 2);
    std::memcpy(data_.data() + offset, &r, sizeof(Rect));
    std::memcpy(data_.data() + offset + sizeof(Rect), &rx, sizeof(f32));
    std::memcpy(data_.data() + offset + sizeof(Rect) + sizeof(f32), &ry, sizeof(f32));
    return offset;
}

void DrawOpArena::getRoundRect(u32 offset, Rect& r, f32& rx, f32& ry) const {
    std::memcpy(&r, data_.data() + offset, sizeof(Rect));
    std::memcpy(&rx, data_.data() + offset + sizeof(Rect), sizeof(f32));
    std::memcpy(&ry, data_.data() + offset + sizeof(Rect) + sizeof(f32), sizeof(f32));
}

void DrawOpArena::reset() {
    data_.clear();
}

// --- Recording ---

Recording::Recording(std::vector<CompactDrawOp> ops, DrawOpArena arena,
                     std::vector<std::shared_ptr<Image>> images)
    : ops_(std::move(ops)), arena_(std::move(arena)), images_(std::move(images)) {
}

const Image* Recording::getImage(u32 index) const {
    if (index < images_.size()) {
        return images_[index].get();
    }
    return nullptr;
}

void Recording::dispatchOp(const CompactDrawOp& op, DrawOpVisitor& visitor) const {
    switch (op.type) {
        case DrawOp::Type::FillRect:
            visitor.visitFillRect(op.data.fill.rect, op.color, op.blendMode, op.opacity);
            break;
        case DrawOp::Type::StrokeRect:
            visitor.visitStrokeRect(op.data.stroke.rect, op.color, op.width, op.blendMode, op.opacity);
            break;
        case DrawOp::Type::Line:
            visitor.visitLine(op.data.line.p1, op.data.line.p2, op.color, op.width, op.blendMode, op.opacity);
            break;
        case DrawOp::Type::Polyline:
            visitor.visitPolyline(
                arena_.getPoints(op.data.polyline.offset),
                static_cast<i32>(op.data.polyline.count),
                op.color, op.width, op.blendMode, op.opacity);
            break;
        case DrawOp::Type::Text:
            visitor.visitText(
                op.data.text.pos,
                arena_.getString(op.data.text.offset),
                op.data.text.len, op.color, op.blendMode, op.opacity);
            break;
        case DrawOp::Type::DrawImage:
            visitor.visitDrawImage(
                getImage(op.data.image.imageIndex),
                op.data.image.x, op.data.image.y, op.blendMode, op.opacity);
            break;
        case DrawOp::Type::SetClip:
            visitor.visitSetClip(op.data.clip.rect);
            break;
        case DrawOp::Type::ClearClip:
            visitor.visitClearClip();
            break;
        case DrawOp::Type::SetTransform:
            visitor.visitSetTransform(*arena_.getMatrix(op.data.transform.matrixOffset));
            break;
        case DrawOp::Type::ClearTransform:
            visitor.visitClearTransform();
            break;
        case DrawOp::Type::FillCircle:
            visitor.visitFillCircle(op.data.circle.cx, op.data.circle.cy,
                                   op.data.circle.radius, op.color, op.blendMode, op.opacity);
            break;
        case DrawOp::Type::StrokeCircle:
            visitor.visitStrokeCircle(op.data.circle.cx, op.data.circle.cy,
                                     op.data.circle.radius, op.color, op.width, op.blendMode, op.opacity);
            break;
        case DrawOp::Type::FillRoundRect: {
            Rect r; f32 rx, ry;
            arena_.getRoundRect(op.data.roundRect.arenaOffset, r, rx, ry);
            visitor.visitFillRoundRect(r, rx, ry, op.color, op.blendMode, op.opacity);
            break;
        }
        case DrawOp::Type::StrokeRoundRect: {
            Rect r; f32 rx, ry;
            arena_.getRoundRect(op.data.roundRect.arenaOffset, r, rx, ry);
            visitor.visitStrokeRoundRect(r, rx, ry, op.color, op.width, op.blendMode, op.opacity);
            break;
        }
    }
}

void Recording::accept(DrawOpVisitor& visitor) const {
    for (const auto& op : ops_) {
        dispatchOp(op, visitor);
    }
}

void Recording::dispatch(DrawOpVisitor& visitor, const DrawPass& pass) const {
    for (u32 idx : pass.sortedIndices()) {
        dispatchOp(ops_[idx], visitor);
    }
}

// --- Recorder ---

void Recorder::reset() {
    ops_.clear();
    arena_.reset();
    images_.clear();
}

void Recorder::fillRect(Rect r, Color c) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::FillRect;
    op.color = c;
    op.data.fill.rect = r;
    ops_.push_back(op);
}

void Recorder::strokeRect(Rect r, Color c, f32 width) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::StrokeRect;
    op.color = c;
    op.width = width;
    op.data.stroke.rect = r;
    ops_.push_back(op);
}

void Recorder::drawLine(Point p1, Point p2, Color c, f32 width) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::Line;
    op.color = c;
    op.width = width;
    op.data.line.p1 = p1;
    op.data.line.p2 = p2;
    ops_.push_back(op);
}

void Recorder::drawPolyline(const Point* pts, i32 count, Color c, f32 width) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::Polyline;
    op.color = c;
    op.width = width;
    op.data.polyline.offset = arena_.storePoints(pts, count);
    op.data.polyline.count = static_cast<u32>(count);
    ops_.push_back(op);
}

void Recorder::drawText(Point p, std::string_view text, Color c) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::Text;
    op.color = c;
    op.data.text.pos = p;
    op.data.text.offset = arena_.storeString(text);
    op.data.text.len = static_cast<u32>(text.size());
    ops_.push_back(op);
}

void Recorder::drawImage(std::shared_ptr<Image> image, f32 x, f32 y) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::DrawImage;
    op.data.image.x = x;
    op.data.image.y = y;
    op.data.image.imageIndex = static_cast<u32>(images_.size());
    images_.push_back(std::move(image));
    ops_.push_back(op);
}

void Recorder::setClip(Rect r) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::SetClip;
    op.data.clip.rect = r;
    ops_.push_back(op);
}

void Recorder::clearClip() {
    CompactDrawOp op{};
    op.type = DrawOp::Type::ClearClip;
    ops_.push_back(op);
}

void Recorder::setTransform(const Matrix& m) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::SetTransform;
    op.data.transform.matrixOffset = arena_.storeMatrix(m);
    ops_.push_back(op);
}

void Recorder::clearTransform() {
    CompactDrawOp op{};
    op.type = DrawOp::Type::ClearTransform;
    ops_.push_back(op);
}

void Recorder::fillCircle(f32 cx, f32 cy, f32 radius, Color c) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::FillCircle;
    op.color = c;
    op.data.circle.cx = cx;
    op.data.circle.cy = cy;
    op.data.circle.radius = radius;
    ops_.push_back(op);
}

void Recorder::strokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::StrokeCircle;
    op.color = c;
    op.width = width;
    op.data.circle.cx = cx;
    op.data.circle.cy = cy;
    op.data.circle.radius = radius;
    ops_.push_back(op);
}

void Recorder::fillRoundRect(Rect r, f32 rx, f32 ry, Color c) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::FillRoundRect;
    op.color = c;
    op.data.roundRect.arenaOffset = arena_.storeRoundRect(r, rx, ry);
    ops_.push_back(op);
}

void Recorder::strokeRoundRect(Rect r, f32 rx, f32 ry, Color c, f32 width) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::StrokeRoundRect;
    op.color = c;
    op.width = width;
    op.data.roundRect.arenaOffset = arena_.storeRoundRect(r, rx, ry);
    ops_.push_back(op);
}

// --- Paint-based overloads ---

void Recorder::fillRect(Rect r, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::FillRect;
    op.color = p.color;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.fill.rect = r;
    ops_.push_back(op);
}

void Recorder::strokeRect(Rect r, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::StrokeRect;
    op.color = p.color;
    op.width = p.strokeWidth;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.stroke.rect = r;
    ops_.push_back(op);
}

void Recorder::drawLine(Point p1, Point p2, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::Line;
    op.color = p.color;
    op.width = p.strokeWidth;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.line.p1 = p1;
    op.data.line.p2 = p2;
    ops_.push_back(op);
}

void Recorder::drawPolyline(const Point* pts, i32 count, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::Polyline;
    op.color = p.color;
    op.width = p.strokeWidth;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.polyline.offset = arena_.storePoints(pts, count);
    op.data.polyline.count = static_cast<u32>(count);
    ops_.push_back(op);
}

void Recorder::drawText(Point pos, std::string_view text, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::Text;
    op.color = p.color;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.text.pos = pos;
    op.data.text.offset = arena_.storeString(text);
    op.data.text.len = static_cast<u32>(text.size());
    ops_.push_back(op);
}

void Recorder::fillCircle(f32 cx, f32 cy, f32 radius, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::FillCircle;
    op.color = p.color;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.circle.cx = cx;
    op.data.circle.cy = cy;
    op.data.circle.radius = radius;
    ops_.push_back(op);
}

void Recorder::strokeCircle(f32 cx, f32 cy, f32 radius, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::StrokeCircle;
    op.color = p.color;
    op.width = p.strokeWidth;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.circle.cx = cx;
    op.data.circle.cy = cy;
    op.data.circle.radius = radius;
    ops_.push_back(op);
}

void Recorder::fillRoundRect(Rect r, f32 rx, f32 ry, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::FillRoundRect;
    op.color = p.color;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.roundRect.arenaOffset = arena_.storeRoundRect(r, rx, ry);
    ops_.push_back(op);
}

void Recorder::strokeRoundRect(Rect r, f32 rx, f32 ry, const Paint& p) {
    CompactDrawOp op{};
    op.type = DrawOp::Type::StrokeRoundRect;
    op.color = p.color;
    op.width = p.strokeWidth;
    op.blendMode = p.blendMode;
    op.opacity = u8(p.opacity * 255);
    op.data.roundRect.arenaOffset = arena_.storeRoundRect(r, rx, ry);
    ops_.push_back(op);
}

std::unique_ptr<Recording> Recorder::finish() {
    return std::make_unique<Recording>(std::move(ops_), std::move(arena_), std::move(images_));
}

}
