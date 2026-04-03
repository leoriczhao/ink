#include <gtest/gtest.h>
#include <ink/surface.hpp>
#include <ink/canvas.hpp>
#include <ink/device.hpp>
#include <ink/recording.hpp>
#include <ink/draw_pass.hpp>
#include <ink/matrix.hpp>

using namespace ink;

// --- Canvas transform API ---

TEST(Transform, TranslateRecordsSetTransform) {
    Device device;
    Canvas canvas(&device);
    device.beginFrame();
    canvas.translate(10, 20);
    device.endFrame();
    auto rec = device.finishRecording();
    ASSERT_NE(rec, nullptr);
    // Should have a SetTransform op
    bool found = false;
    for (auto& op : rec->ops()) {
        if (op.type == DrawOp::Type::SetTransform) found = true;
    }
    EXPECT_TRUE(found);
}

TEST(Transform, SaveRestoreTransform) {
    Device device;
    Canvas canvas(&device);
    device.beginFrame();

    canvas.translate(10, 20);
    Matrix m1 = canvas.getMatrix();
    EXPECT_FALSE(m1.isIdentity());

    canvas.save();
    canvas.translate(5, 5);
    Matrix m2 = canvas.getMatrix();
    EXPECT_NE(m1, m2);

    canvas.restore();
    Matrix m3 = canvas.getMatrix();
    EXPECT_EQ(m1, m3);
}

TEST(Transform, IdentityDoesNotRecordOp) {
    Device device;
    Canvas canvas(&device);
    device.beginFrame();
    canvas.setMatrix(Matrix::Identity());
    device.endFrame();
    auto rec = device.finishRecording();
    // Identity should produce ClearTransform, not SetTransform
    bool hasSet = false;
    for (auto& op : rec->ops()) {
        if (op.type == DrawOp::Type::SetTransform) hasSet = true;
    }
    EXPECT_FALSE(hasSet);
}

TEST(Transform, ScaleAndRotateCompose) {
    Device device;
    Canvas canvas(&device);
    canvas.scale(2, 2);
    canvas.rotate(0.5f);
    Matrix m = canvas.getMatrix();
    EXPECT_FALSE(m.isIdentity());
    EXPECT_FALSE(m.isScaleTranslateOnly());
}

// --- Surface-level transform rendering ---

TEST(Transform, TranslatedFillRectMovesPixels) {
    auto surface = Surface::MakeRaster(20, 20);
    surface->beginFrame({0, 0, 0, 255});
    auto* canvas = surface->canvas();

    // Translate by (10, 10) then fill a 5x5 rect at (0,0)
    canvas->translate(10, 10);
    canvas->fillRect({0, 0, 5, 5}, {255, 0, 0, 255});

    surface->endFrame();
    surface->flush();

    auto* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);

    // Pixel at (0,0) should be background (black)
    u32 bg = *static_cast<const u32*>(pm->rowAddr(0));
    // Pixel at (12,12) should be red (inside translated rect)
    u32 px = static_cast<const u32*>(pm->rowAddr(12))[12];
    EXPECT_NE(bg, px);
}

TEST(Transform, ScaledFillRectDoubles) {
    auto surface = Surface::MakeRaster(20, 20);
    surface->beginFrame({0, 0, 0, 255});
    auto* canvas = surface->canvas();

    // Scale by 2x then fill a 3x3 rect at (0,0) -> should become 6x6
    canvas->scale(2, 2);
    canvas->fillRect({0, 0, 3, 3}, {0, 255, 0, 255});

    surface->endFrame();
    surface->flush();

    auto* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);

    // Pixel at (5,5) should be green (inside 6x6 scaled rect)
    u32 pxInside = static_cast<const u32*>(pm->rowAddr(5))[5];
    // Pixel at (7,7) should be background
    u32 pxOutside = static_cast<const u32*>(pm->rowAddr(7))[7];
    EXPECT_NE(pxInside, pxOutside);
}

TEST(Transform, TranslatedLineMovesEndpoints) {
    auto surface = Surface::MakeRaster(20, 20);
    surface->beginFrame({0, 0, 0, 255});
    auto* canvas = surface->canvas();

    canvas->translate(10, 10);
    canvas->drawLine({0, 0}, {5, 0}, {255, 255, 0, 255}, 1.0f);

    surface->endFrame();
    surface->flush();

    auto* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);

    // Pixel at (12,10) should be colored (translated line)
    u32 pxLine = static_cast<const u32*>(pm->rowAddr(10))[12];
    // Pixel at (0,0) should be background
    u32 pxBg = static_cast<const u32*>(pm->rowAddr(0))[0];
    EXPECT_NE(pxLine, pxBg);
}
