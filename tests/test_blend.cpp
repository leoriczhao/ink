#include <gtest/gtest.h>
#include <ink/surface.hpp>
#include <ink/canvas.hpp>
#include <ink/paint.hpp>

using namespace ink;

static u32 readPixel(const Pixmap* pm, i32 x, i32 y) {
    return static_cast<const u32*>(pm->rowAddr(y))[x];
}

// Helper: fill background, then fill foreground with blend mode, return result pixel
static u32 blendTest(Color bg, Color fg, BlendMode mode) {
    auto surface = Surface::MakeRaster(4, 4);
    surface->beginFrame(bg);

    Paint p = Paint::Fill(fg);
    p.blendMode = mode;
    surface->canvas()->draw({0, 0, 4, 4}, p);

    surface->endFrame();
    surface->flush();
    return readPixel(surface->peekPixels(), 0, 0);
}

TEST(Blend, SrcOverMatchesDefault) {
    // Red over black with 50% alpha should blend
    u32 px = blendTest({0,0,0,255}, {255,0,0,128}, BlendMode::SrcOver);
    // Should be reddish (not pure red, not pure black)
    u8 r = (px >> 16) & 0xFF;
    EXPECT_GT(r, 50);
    EXPECT_LT(r, 200);
}

TEST(Blend, SrcReplacesCompletely) {
    // Src mode: fully replace destination
    u32 px = blendTest({0,0,0,255}, {255,0,0,255}, BlendMode::Src);
    // Should be pure red (0xFFFF0000 in BGRA is 0xFF0000FF)
    u8 r = (px >> 16) & 0xFF;
    u8 g = (px >> 8) & 0xFF;
    u8 b = px & 0xFF;
    EXPECT_EQ(r, 255);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(b, 0);
}

TEST(Blend, DstKeepsDestination) {
    // Dst mode: keep destination, ignore source
    u32 px = blendTest({0,255,0,255}, {255,0,0,255}, BlendMode::Dst);
    u8 r = (px >> 16) & 0xFF;
    u8 g = (px >> 8) & 0xFF;
    EXPECT_EQ(r, 0);
    EXPECT_EQ(g, 255);
}

TEST(Blend, ClearProducesTransparent) {
    u32 px = blendTest({255,255,255,255}, {255,0,0,255}, BlendMode::Clear);
    EXPECT_EQ(px, 0u);
}

TEST(Blend, DstOutRemovesWhereSourceExists) {
    // DstOut: destination visible only where source is transparent
    // Source is fully opaque -> destination should be removed
    u32 px = blendTest({0,255,0,255}, {255,0,0,255}, BlendMode::DstOut);
    u8 a = (px >> 24) & 0xFF;
    EXPECT_EQ(a, 0);
}

TEST(Blend, SrcInOnlyWhereDestinationExists) {
    // SrcIn: source visible only where destination has alpha
    // Destination is opaque -> source should be fully visible
    u32 px = blendTest({0,0,0,255}, {255,0,0,255}, BlendMode::SrcIn);
    u8 r = (px >> 16) & 0xFF;
    EXPECT_EQ(r, 255);
}
