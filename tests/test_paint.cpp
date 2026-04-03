#include <gtest/gtest.h>
#include <ink/paint.hpp>
#include <ink/surface.hpp>

using namespace ink;

// --- Paint construction ---

TEST(Paint, FillFromColor) {
    auto p = Paint::Fill({255, 0, 0, 255});
    EXPECT_EQ(p.color.r, 255);
    EXPECT_EQ(p.color.a, 255);
    EXPECT_EQ(p.style, PaintStyle::Fill);
    EXPECT_EQ(p.blendMode, BlendMode::SrcOver);
    EXPECT_FLOAT_EQ(p.opacity, 1.0f);
}

TEST(Paint, StrokeFromColor) {
    auto p = Paint::Stroke({0, 255, 0, 255}, 3.0f);
    EXPECT_EQ(p.color.g, 255);
    EXPECT_EQ(p.style, PaintStyle::Stroke);
    EXPECT_FLOAT_EQ(p.strokeWidth, 3.0f);
}

TEST(Paint, EffectiveAlpha) {
    Paint p;
    p.color = {255, 0, 0, 200};
    p.opacity = 0.5f;
    EXPECT_EQ(p.effectiveAlpha(), 100);  // 200 * 0.5 = 100
}

TEST(Paint, DefaultBlendMode) {
    Paint p;
    EXPECT_EQ(p.blendMode, BlendMode::SrcOver);
}

// --- Surface with Paint ---

TEST(Paint, FillRectWithPaintProducesPixels) {
    auto surface = Surface::MakeRaster(10, 10);
    surface->beginFrame({0, 0, 0, 255});

    Paint p = Paint::Fill({255, 0, 0, 255});
    surface->canvas()->draw({0, 0, 10, 10}, p);

    surface->endFrame();
    surface->flush();

    auto* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);
    // Should not be background (black)
    u32 pixel = static_cast<const u32*>(pm->rowAddr(5))[5];
    EXPECT_NE(pixel, 0xFF000000u);
}

TEST(Paint, HalfOpacityProducesBlendedPixel) {
    auto surface = Surface::MakeRaster(10, 10);
    surface->beginFrame({0, 0, 0, 255});

    Paint p = Paint::Fill({255, 255, 255, 255});
    p.opacity = 0.5f;
    surface->canvas()->draw({0, 0, 10, 10}, p);

    surface->endFrame();
    surface->flush();

    auto* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);
    u32 pixel = static_cast<const u32*>(pm->rowAddr(5))[5];
    // Should be neither pure white nor pure black (blended)
    EXPECT_NE(pixel, 0xFFFFFFFFu);
    EXPECT_NE(pixel, 0xFF000000u);
}
