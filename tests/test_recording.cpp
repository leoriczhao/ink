#include <gtest/gtest.h>
#include <ink/recording.hpp>
#include <ink/draw_op_visitor.hpp>
#include <ink/image.hpp>

#include <string>
#include <vector>

using namespace ink;

// --- Mock visitor that records which methods were called ---

struct VisitorCall {
    enum class Kind {
        FillRect, StrokeRect, Line, Polyline, Text, DrawImage, SetClip, ClearClip
    };
    Kind kind;
};

class MockVisitor : public DrawOpVisitor {
public:
    std::vector<VisitorCall> calls;

    void visitFillRect(Rect, Color) override {
        calls.push_back({VisitorCall::Kind::FillRect});
    }
    void visitStrokeRect(Rect, Color, f32) override {
        calls.push_back({VisitorCall::Kind::StrokeRect});
    }
    void visitLine(Point, Point, Color, f32) override {
        calls.push_back({VisitorCall::Kind::Line});
    }
    void visitPolyline(const Point*, i32, Color, f32) override {
        calls.push_back({VisitorCall::Kind::Polyline});
    }
    void visitText(Point, const char*, u32, Color) override {
        calls.push_back({VisitorCall::Kind::Text});
    }
    void visitDrawImage(const Image*, f32, f32) override {
        calls.push_back({VisitorCall::Kind::DrawImage});
    }
    void visitSetClip(Rect) override {
        calls.push_back({VisitorCall::Kind::SetClip});
    }
    void visitClearClip() override {
        calls.push_back({VisitorCall::Kind::ClearClip});
    }
};

// --- Recorder starts empty ---

TEST(Recording, RecorderStartsEmpty) {
    Recorder rec;
    auto recording = rec.finish();
    ASSERT_NE(recording, nullptr);
    EXPECT_EQ(recording->ops().size(), 0u);
}

// --- fillRect ---

TEST(Recording, FillRectRecorded) {
    Recorder rec;
    Color c{255, 0, 0, 255};
    rec.fillRect({10, 20, 100, 200}, c);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::FillRect);
    EXPECT_EQ(op.color.r, 255);
    EXPECT_EQ(op.color.g, 0);
    EXPECT_EQ(op.color.b, 0);
    EXPECT_EQ(op.color.a, 255);
    EXPECT_FLOAT_EQ(op.data.fill.rect.x, 10.0f);
    EXPECT_FLOAT_EQ(op.data.fill.rect.y, 20.0f);
    EXPECT_FLOAT_EQ(op.data.fill.rect.w, 100.0f);
    EXPECT_FLOAT_EQ(op.data.fill.rect.h, 200.0f);
}

// --- strokeRect with width ---

TEST(Recording, StrokeRectRecordedWithWidth) {
    Recorder rec;
    Color c{0, 128, 0, 255};
    rec.strokeRect({5, 10, 50, 60}, c, 3.5f);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::StrokeRect);
    EXPECT_FLOAT_EQ(op.width, 3.5f);
    EXPECT_EQ(op.color.g, 128);
    EXPECT_FLOAT_EQ(op.data.stroke.rect.x, 5.0f);
    EXPECT_FLOAT_EQ(op.data.stroke.rect.w, 50.0f);
}

// --- drawLine with two points ---

TEST(Recording, DrawLineRecorded) {
    Recorder rec;
    Color c{0, 0, 255, 255};
    rec.drawLine({1.0f, 2.0f}, {3.0f, 4.0f}, c, 2.0f);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::Line);
    EXPECT_FLOAT_EQ(op.data.line.p1.x, 1.0f);
    EXPECT_FLOAT_EQ(op.data.line.p1.y, 2.0f);
    EXPECT_FLOAT_EQ(op.data.line.p2.x, 3.0f);
    EXPECT_FLOAT_EQ(op.data.line.p2.y, 4.0f);
    EXPECT_FLOAT_EQ(op.width, 2.0f);
    EXPECT_EQ(op.color.b, 255);
}

// --- drawPolyline, points stored in arena ---

TEST(Recording, DrawPolylinePointsInArena) {
    Recorder rec;
    Point pts[] = {{0, 0}, {10, 20}, {30, 40}, {50, 60}};
    Color c{100, 100, 100, 255};
    rec.drawPolyline(pts, 4, c, 1.0f);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::Polyline);
    EXPECT_EQ(op.data.polyline.count, 4u);

    const Point* stored = recording->arena().getPoints(op.data.polyline.offset);
    ASSERT_NE(stored, nullptr);
    EXPECT_FLOAT_EQ(stored[0].x, 0.0f);
    EXPECT_FLOAT_EQ(stored[0].y, 0.0f);
    EXPECT_FLOAT_EQ(stored[1].x, 10.0f);
    EXPECT_FLOAT_EQ(stored[1].y, 20.0f);
    EXPECT_FLOAT_EQ(stored[2].x, 30.0f);
    EXPECT_FLOAT_EQ(stored[2].y, 40.0f);
    EXPECT_FLOAT_EQ(stored[3].x, 50.0f);
    EXPECT_FLOAT_EQ(stored[3].y, 60.0f);
}

// --- drawText, string stored in arena ---

TEST(Recording, DrawTextStringInArena) {
    Recorder rec;
    Color c{0, 0, 0, 255};
    rec.drawText({10, 20}, "Hello, ink!", c);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::Text);
    EXPECT_FLOAT_EQ(op.data.text.pos.x, 10.0f);
    EXPECT_FLOAT_EQ(op.data.text.pos.y, 20.0f);
    EXPECT_EQ(op.data.text.len, 11u);

    const char* stored = recording->arena().getString(op.data.text.offset);
    ASSERT_NE(stored, nullptr);
    EXPECT_STREQ(stored, "Hello, ink!");
}

// --- drawImage with image index ---

TEST(Recording, DrawImageRecorded) {
    // Create a minimal pixmap to make a valid Image
    Pixmap pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(2, 2));
    auto img = Image::MakeFromPixmap(pm);

    Recorder rec;
    rec.drawImage(img, 15.0f, 25.0f);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::DrawImage);
    EXPECT_FLOAT_EQ(op.data.image.x, 15.0f);
    EXPECT_FLOAT_EQ(op.data.image.y, 25.0f);
    EXPECT_EQ(op.data.image.imageIndex, 0u);

    // The image should be stored in the recording's image list
    ASSERT_EQ(recording->images().size(), 1u);
    EXPECT_EQ(recording->getImage(0), img.get());
}

// --- setClip and clearClip ---

TEST(Recording, SetClipAndClearClip) {
    Recorder rec;
    rec.setClip({0, 0, 640, 480});
    rec.clearClip();

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 2u);

    EXPECT_EQ(recording->ops()[0].type, DrawOp::Type::SetClip);
    EXPECT_FLOAT_EQ(recording->ops()[0].data.clip.rect.w, 640.0f);
    EXPECT_FLOAT_EQ(recording->ops()[0].data.clip.rect.h, 480.0f);

    EXPECT_EQ(recording->ops()[1].type, DrawOp::Type::ClearClip);
}

// --- reset() clears all ops ---

TEST(Recording, ResetClearsOps) {
    Recorder rec;
    rec.fillRect({0, 0, 10, 10}, {255, 0, 0, 255});
    rec.fillRect({0, 0, 20, 20}, {0, 255, 0, 255});
    rec.reset();

    auto recording = rec.finish();
    ASSERT_NE(recording, nullptr);
    EXPECT_EQ(recording->ops().size(), 0u);
}

// --- accept() calls visitor methods in order ---

TEST(Recording, AcceptCallsVisitorInOrder) {
    Recorder rec;
    rec.fillRect({0, 0, 10, 10}, {255, 0, 0, 255});
    rec.drawLine({0, 0}, {1, 1}, {0, 0, 0, 255}, 1.0f);
    rec.setClip({0, 0, 100, 100});
    rec.strokeRect({5, 5, 20, 20}, {0, 255, 0, 255}, 2.0f);
    rec.clearClip();

    auto recording = rec.finish();

    MockVisitor visitor;
    recording->accept(visitor);

    ASSERT_EQ(visitor.calls.size(), 5u);
    EXPECT_EQ(visitor.calls[0].kind, VisitorCall::Kind::FillRect);
    EXPECT_EQ(visitor.calls[1].kind, VisitorCall::Kind::Line);
    EXPECT_EQ(visitor.calls[2].kind, VisitorCall::Kind::SetClip);
    EXPECT_EQ(visitor.calls[3].kind, VisitorCall::Kind::StrokeRect);
    EXPECT_EQ(visitor.calls[4].kind, VisitorCall::Kind::ClearClip);
}

// --- Multiple ops maintain order ---

TEST(Recording, MultipleOpsPreserveOrder) {
    Recorder rec;
    rec.fillRect({0, 0, 1, 1}, {1, 0, 0, 255});
    rec.fillRect({0, 0, 2, 2}, {2, 0, 0, 255});
    rec.strokeRect({0, 0, 3, 3}, {3, 0, 0, 255}, 1.0f);
    rec.drawLine({0, 0}, {4, 4}, {4, 0, 0, 255}, 1.0f);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 4u);

    EXPECT_EQ(recording->ops()[0].type, DrawOp::Type::FillRect);
    EXPECT_EQ(recording->ops()[0].color.r, 1);

    EXPECT_EQ(recording->ops()[1].type, DrawOp::Type::FillRect);
    EXPECT_EQ(recording->ops()[1].color.r, 2);

    EXPECT_EQ(recording->ops()[2].type, DrawOp::Type::StrokeRect);
    EXPECT_EQ(recording->ops()[2].color.r, 3);

    EXPECT_EQ(recording->ops()[3].type, DrawOp::Type::Line);
    EXPECT_EQ(recording->ops()[3].color.r, 4);
}
