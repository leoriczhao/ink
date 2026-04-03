#include <gtest/gtest.h>
#include <ink/recording.hpp>
#include <ink/draw_op_visitor.hpp>
#include <ink/draw_pass.hpp>
#include <ink/paint.hpp>
#include <ink/pixmap.hpp>
#include <ink/image.hpp>
#include "../src/cpu_renderer.hpp"

#include <cmath>

using namespace ink;

// --- Recording tests for new shape types ---

TEST(Shapes, FillCircleRecorded) {
    Recorder rec;
    Color c{255, 0, 0, 255};
    rec.fillCircle(50.0f, 60.0f, 25.0f, c);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::FillCircle);
    EXPECT_EQ(op.color.r, 255);
    EXPECT_FLOAT_EQ(op.data.circle.cx, 50.0f);
    EXPECT_FLOAT_EQ(op.data.circle.cy, 60.0f);
    EXPECT_FLOAT_EQ(op.data.circle.radius, 25.0f);
}

TEST(Shapes, StrokeCircleRecorded) {
    Recorder rec;
    Color c{0, 255, 0, 255};
    rec.strokeCircle(100.0f, 100.0f, 40.0f, c, 3.0f);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::StrokeCircle);
    EXPECT_EQ(op.color.g, 255);
    EXPECT_FLOAT_EQ(op.width, 3.0f);
    EXPECT_FLOAT_EQ(op.data.circle.cx, 100.0f);
    EXPECT_FLOAT_EQ(op.data.circle.cy, 100.0f);
    EXPECT_FLOAT_EQ(op.data.circle.radius, 40.0f);
}

TEST(Shapes, FillRoundRectRecorded) {
    Recorder rec;
    Color c{0, 0, 255, 255};
    rec.fillRoundRect({10, 20, 100, 80}, 8.0f, 8.0f, c);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::FillRoundRect);
    EXPECT_EQ(op.color.b, 255);

    // Retrieve from arena
    Rect r; f32 rx, ry;
    recording->arena().getRoundRect(op.data.roundRect.arenaOffset, r, rx, ry);
    EXPECT_FLOAT_EQ(r.x, 10.0f);
    EXPECT_FLOAT_EQ(r.y, 20.0f);
    EXPECT_FLOAT_EQ(r.w, 100.0f);
    EXPECT_FLOAT_EQ(r.h, 80.0f);
    EXPECT_FLOAT_EQ(rx, 8.0f);
    EXPECT_FLOAT_EQ(ry, 8.0f);
}

TEST(Shapes, StrokeRoundRectRecorded) {
    Recorder rec;
    Color c{128, 128, 0, 255};
    rec.strokeRoundRect({5, 10, 50, 40}, 6.0f, 6.0f, c, 2.0f);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::StrokeRoundRect);
    EXPECT_FLOAT_EQ(op.width, 2.0f);

    Rect r; f32 rx, ry;
    recording->arena().getRoundRect(op.data.roundRect.arenaOffset, r, rx, ry);
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.w, 50.0f);
    EXPECT_FLOAT_EQ(rx, 6.0f);
    EXPECT_FLOAT_EQ(ry, 6.0f);
}

TEST(Shapes, PaintBasedCircleRecorded) {
    Recorder rec;
    Paint p;
    p.color = {255, 128, 0, 255};
    p.blendMode = BlendMode::SrcOver;
    p.opacity = 0.5f;
    rec.fillCircle(30.0f, 40.0f, 15.0f, p);

    auto recording = rec.finish();
    ASSERT_EQ(recording->ops().size(), 1u);

    const auto& op = recording->ops()[0];
    EXPECT_EQ(op.type, DrawOp::Type::FillCircle);
    EXPECT_EQ(op.blendMode, BlendMode::SrcOver);
    EXPECT_EQ(op.opacity, 127);  // 0.5 * 255
    EXPECT_FLOAT_EQ(op.data.circle.radius, 15.0f);
}

// --- CPU rendering tests ---

TEST(Shapes, FillCircleCenterPixelIsColored) {
    // Create a 100x100 pixmap, fill a circle at center (50,50) with r=20
    Pixmap pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(100, 100));
    CpuRenderer renderer(&pm);
    renderer.beginFrame({0, 0, 0, 255});

    Recorder rec;
    rec.fillCircle(50.0f, 50.0f, 20.0f, {255, 0, 0, 255});
    auto recording = rec.finish();
    auto pass = DrawPass::create(*recording);
    renderer.execute(*recording, pass);

    // Center pixel should be red
    const u32* pixels = static_cast<const u32*>(pm.addr());
    u32 centerPixel = pixels[50 * 100 + 50];
    // RGBA format: R is lowest byte
    u8 r = centerPixel & 0xFF;
    EXPECT_EQ(r, 255);

    // Corner pixel (0,0) should still be black (background)
    u32 cornerPixel = pixels[0];
    u8 cornerR = cornerPixel & 0xFF;
    EXPECT_EQ(cornerR, 0);
}

TEST(Shapes, StrokeCircleRingPixels) {
    // Create a 100x100 pixmap, stroke circle at (50,50) r=30 w=4
    Pixmap pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(100, 100));
    CpuRenderer renderer(&pm);
    renderer.beginFrame({0, 0, 0, 255});

    Recorder rec;
    rec.strokeCircle(50.0f, 50.0f, 30.0f, {0, 255, 0, 255}, 4.0f);
    auto recording = rec.finish();
    auto pass = DrawPass::create(*recording);
    renderer.execute(*recording, pass);

    const u32* pixels = static_cast<const u32*>(pm.addr());

    // A pixel on the ring (50+30, 50) = (80, 50) should be green
    u32 ringPixel = pixels[50 * 100 + 80];
    u8 ringG = (ringPixel >> 8) & 0xFF;
    EXPECT_EQ(ringG, 255);

    // Center pixel (50,50) should be black (not filled)
    u32 centerPixel = pixels[50 * 100 + 50];
    u8 centerG = (centerPixel >> 8) & 0xFF;
    EXPECT_EQ(centerG, 0);
}

TEST(Shapes, FillRoundRectCornersDifferFromRect) {
    // Fill a regular rect and a rounded rect of the same size, compare corner pixel
    // The rounded rect should NOT fill the extreme corner pixel.

    // Regular rect: fill entire 100x80 at (10,10)
    Pixmap pm1 = Pixmap::Alloc(PixmapInfo::MakeRGBA(120, 100));
    CpuRenderer r1(&pm1);
    r1.beginFrame({0, 0, 0, 255});
    {
        Recorder rec;
        rec.fillRect({10, 10, 100, 80}, {255, 0, 0, 255});
        auto recording = rec.finish();
        auto pass = DrawPass::create(*recording);
        r1.execute(*recording, pass);
    }

    // Rounded rect: same rect but with rx=ry=15
    Pixmap pm2 = Pixmap::Alloc(PixmapInfo::MakeRGBA(120, 100));
    CpuRenderer r2(&pm2);
    r2.beginFrame({0, 0, 0, 255});
    {
        Recorder rec;
        rec.fillRoundRect({10, 10, 100, 80}, 15.0f, 15.0f, {255, 0, 0, 255});
        auto recording = rec.finish();
        auto pass = DrawPass::create(*recording);
        r2.execute(*recording, pass);
    }

    const u32* px1 = static_cast<const u32*>(pm1.addr());
    const u32* px2 = static_cast<const u32*>(pm2.addr());

    // Top-left corner of the rect at (10,10) - the regular rect should have it colored
    u32 rectCorner = px1[10 * 120 + 10];
    u8 rectR = rectCorner & 0xFF;
    EXPECT_EQ(rectR, 255);

    // The rounded rect should NOT have the extreme corner colored (it's in the rounded-off area)
    u32 roundCorner = px2[10 * 120 + 10];
    u8 roundR = roundCorner & 0xFF;
    EXPECT_EQ(roundR, 0);

    // But the center of the rounded rect should be colored
    u32 roundCenter = px2[50 * 120 + 60];
    u8 roundCenterR = roundCenter & 0xFF;
    EXPECT_EQ(roundCenterR, 255);
}

// --- Visitor dispatch tests ---

struct ShapeMockVisitor : public DrawOpVisitor {
    std::vector<DrawOp::Type> types;

    void visitFillRect(Rect, Color, BlendMode, u8) override {
        types.push_back(DrawOp::Type::FillRect);
    }
    void visitStrokeRect(Rect, Color, f32, BlendMode, u8) override {
        types.push_back(DrawOp::Type::StrokeRect);
    }
    void visitLine(Point, Point, Color, f32, BlendMode, u8) override {
        types.push_back(DrawOp::Type::Line);
    }
    void visitPolyline(const Point*, i32, Color, f32, BlendMode, u8) override {
        types.push_back(DrawOp::Type::Polyline);
    }
    void visitText(Point, const char*, u32, Color, BlendMode, u8) override {
        types.push_back(DrawOp::Type::Text);
    }
    void visitDrawImage(const Image*, f32, f32, BlendMode, u8) override {
        types.push_back(DrawOp::Type::DrawImage);
    }
    void visitSetClip(Rect) override {
        types.push_back(DrawOp::Type::SetClip);
    }
    void visitClearClip() override {
        types.push_back(DrawOp::Type::ClearClip);
    }
    void visitSetTransform(const Matrix&) override {
        types.push_back(DrawOp::Type::SetTransform);
    }
    void visitClearTransform() override {
        types.push_back(DrawOp::Type::ClearTransform);
    }
    void visitFillCircle(f32, f32, f32, Color, BlendMode, u8) override {
        types.push_back(DrawOp::Type::FillCircle);
    }
    void visitStrokeCircle(f32, f32, f32, Color, f32, BlendMode, u8) override {
        types.push_back(DrawOp::Type::StrokeCircle);
    }
    void visitFillRoundRect(Rect, f32, f32, Color, BlendMode, u8) override {
        types.push_back(DrawOp::Type::FillRoundRect);
    }
    void visitStrokeRoundRect(Rect, f32, f32, Color, f32, BlendMode, u8) override {
        types.push_back(DrawOp::Type::StrokeRoundRect);
    }
};

TEST(Shapes, VisitorDispatchesAllNewTypes) {
    Recorder rec;
    rec.fillCircle(10, 20, 5, {255, 0, 0, 255});
    rec.strokeCircle(30, 40, 10, {0, 255, 0, 255}, 2.0f);
    rec.fillRoundRect({0, 0, 50, 50}, 5, 5, {0, 0, 255, 255});
    rec.strokeRoundRect({10, 10, 40, 40}, 3, 3, {128, 128, 128, 255}, 1.0f);

    auto recording = rec.finish();

    ShapeMockVisitor visitor;
    recording->accept(visitor);

    ASSERT_EQ(visitor.types.size(), 4u);
    EXPECT_EQ(visitor.types[0], DrawOp::Type::FillCircle);
    EXPECT_EQ(visitor.types[1], DrawOp::Type::StrokeCircle);
    EXPECT_EQ(visitor.types[2], DrawOp::Type::FillRoundRect);
    EXPECT_EQ(visitor.types[3], DrawOp::Type::StrokeRoundRect);
}
