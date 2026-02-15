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
    u32 offset = static_cast<u32>(data_.size());
    data_.resize(data_.size() + bytes);
    return offset;
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
            visitor.visitFillRect(op.data.fill.rect, op.color);
            break;
        case DrawOp::Type::StrokeRect:
            visitor.visitStrokeRect(op.data.stroke.rect, op.color, op.width);
            break;
        case DrawOp::Type::Line:
            visitor.visitLine(op.data.line.p1, op.data.line.p2, op.color, op.width);
            break;
        case DrawOp::Type::Polyline:
            visitor.visitPolyline(
                arena_.getPoints(op.data.polyline.offset),
                static_cast<i32>(op.data.polyline.count),
                op.color, op.width);
            break;
        case DrawOp::Type::Text:
            visitor.visitText(
                op.data.text.pos,
                arena_.getString(op.data.text.offset),
                op.data.text.len, op.color);
            break;
        case DrawOp::Type::DrawImage:
            visitor.visitDrawImage(
                getImage(op.data.image.imageIndex),
                op.data.image.x, op.data.image.y);
            break;
        case DrawOp::Type::SetClip:
            visitor.visitSetClip(op.data.clip.rect);
            break;
        case DrawOp::Type::ClearClip:
            visitor.visitClearClip();
            break;
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

std::unique_ptr<Recording> Recorder::finish() {
    return std::make_unique<Recording>(std::move(ops_), std::move(arena_), std::move(images_));
}

}
