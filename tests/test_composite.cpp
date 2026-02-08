#include <gtest/gtest.h>
#include <ink/ink.hpp>
#include <cstring>
#include <vector>

using namespace ink;

// Helper: pack a Color into BGRA8888 u32 (the default surface format)
static u32 packBGRA(Color c) {
    return (u32(c.a) << 24) | (u32(c.r) << 16) | (u32(c.g) << 8) | u32(c.b);
}

// Helper: read a single pixel from a pixmap at (x, y)
static u32 readPixel(const Pixmap* pm, i32 x, i32 y) {
    const u32* row = static_cast<const u32*>(pm->rowAddr(y));
    return row[x];
}

// Helper: create a small raster surface, fill it with a solid color,
// flush, and return a snapshot Image.
static std::shared_ptr<Image> makeSolidImage(i32 w, i32 h, Color c) {
    auto surface = Surface::MakeRaster(w, h);
    surface->beginFrame();
    surface->canvas()->fillRect({0, 0, f32(w), f32(h)}, c);
    surface->endFrame();
    surface->flush();
    return surface->makeSnapshot();
}

// Helper: create an image by directly writing raw pixel values into a pixmap.
// This allows creating images with arbitrary alpha (including fully transparent)
// without going through the rendering pipeline which clears to opaque black.
static std::shared_ptr<Image> makeRawImage(i32 w, i32 h, u32 packedPixel) {
    PixmapInfo info = PixmapInfo::Make(w, h, PixelFormat::BGRA8888);
    Pixmap pm = Pixmap::Alloc(info);
    u32* pixels = pm.addr32();
    i32 total = w * h;
    for (i32 i = 0; i < total; ++i) {
        pixels[i] = packedPixel;
    }
    return Image::MakeFromPixmap(pm);
}

// Helper: compute the expected result of alpha-blending src over dst.
// Uses the same integer formula as CpuBackend::blendPixel.
// Both colors are in logical (R,G,B,A) space.
// The dst is assumed to be opaque after beginFrame clears to black.
static Color alphaBlend(Color src, Color dst) {
    u8 a = src.a;
    u8 outR = u8((src.r * a + dst.r * (255 - a)) / 255);
    u8 outG = u8((src.g * a + dst.g * (255 - a)) / 255);
    u8 outB = u8((src.b * a + dst.b * (255 - a)) / 255);
    return {outR, outG, outB, 255};
}

// Black is the clear color used by beginFrame
static const Color kBlack{0, 0, 0, 255};

// --- Draw opaque image onto empty surface ---

TEST(Composite, OpaqueImageMatchesSource) {
    const i32 W = 4, H = 4;
    Color red{255, 0, 0, 255};

    auto image = makeSolidImage(W, H, red);
    ASSERT_NE(image, nullptr);

    // Destination surface
    auto dst = Surface::MakeRaster(W, H);
    dst->beginFrame();
    dst->canvas()->drawImage(image, 0, 0);
    dst->endFrame();
    dst->flush();

    Pixmap* pm = dst->peekPixels();
    ASSERT_NE(pm, nullptr);

    u32 expected = packBGRA(red);
    for (i32 y = 0; y < H; ++y) {
        for (i32 x = 0; x < W; ++x) {
            EXPECT_EQ(readPixel(pm, x, y), expected)
                << "Mismatch at (" << x << ", " << y << ")";
        }
    }
}

// --- Draw semi-transparent image: pixels are alpha-blended ---

TEST(Composite, SemiTransparentImageIsBlended) {
    const i32 W = 4, H = 4;
    // Semi-transparent green (alpha = 128)
    Color semiGreen{0, 255, 0, 128};

    auto image = makeSolidImage(W, H, semiGreen);
    ASSERT_NE(image, nullptr);

    auto dst = Surface::MakeRaster(W, H);
    dst->beginFrame();  // clears to black (0,0,0,255)
    dst->canvas()->drawImage(image, 0, 0);
    dst->endFrame();
    dst->flush();

    Pixmap* pm = dst->peekPixels();
    ASSERT_NE(pm, nullptr);

    // Expected: semiGreen blended over black
    Color blended = alphaBlend(semiGreen, kBlack);
    u32 expected = packBGRA(blended);

    for (i32 y = 0; y < H; ++y) {
        for (i32 x = 0; x < W; ++x) {
            EXPECT_EQ(readPixel(pm, x, y), expected)
                << "Mismatch at (" << x << ", " << y << ")";
        }
    }
}

// --- Draw image at offset position ---

TEST(Composite, ImageAtOffsetOnlyAffectsRegion) {
    const i32 DST_W = 10, DST_H = 10;
    const i32 IMG_W = 4, IMG_H = 4;
    const i32 OFF_X = 5, OFF_Y = 5;

    Color blue{0, 0, 255, 255};
    auto image = makeSolidImage(IMG_W, IMG_H, blue);
    ASSERT_NE(image, nullptr);

    auto dst = Surface::MakeRaster(DST_W, DST_H);
    dst->beginFrame();
    dst->canvas()->drawImage(image, f32(OFF_X), f32(OFF_Y));
    dst->endFrame();
    dst->flush();

    Pixmap* pm = dst->peekPixels();
    ASSERT_NE(pm, nullptr);

    u32 bluePixel = packBGRA(blue);
    u32 blackPixel = packBGRA(kBlack);

    for (i32 y = 0; y < DST_H; ++y) {
        for (i32 x = 0; x < DST_W; ++x) {
            bool inImage = (x >= OFF_X && x < OFF_X + IMG_W &&
                            y >= OFF_Y && y < OFF_Y + IMG_H);
            u32 expected = inImage ? bluePixel : blackPixel;
            EXPECT_EQ(readPixel(pm, x, y), expected)
                << "Mismatch at (" << x << ", " << y << ")";
        }
    }
}

// --- Composite two layers: bottom visible where top is transparent ---

TEST(Composite, TwoLayersBottomVisibleThroughTransparentTop) {
    const i32 W = 4, H = 4;

    // Bottom layer: solid red, covers full surface
    Color red{255, 0, 0, 255};
    auto bottomImage = makeSolidImage(W, H, red);
    ASSERT_NE(bottomImage, nullptr);

    // Top layer: fully transparent pixels (alpha = 0)
    // We must create this with raw pixel data because the rendering pipeline
    // clears to opaque black on beginFrame, making makeSolidImage unsuitable
    // for producing truly transparent images.
    u32 transparentPixel = 0x00000000;  // ARGB = 0,0,0,0
    auto topImage = makeRawImage(W, H, transparentPixel);
    ASSERT_NE(topImage, nullptr);

    auto dst = Surface::MakeRaster(W, H);
    dst->beginFrame();
    dst->canvas()->drawImage(bottomImage, 0, 0);
    dst->canvas()->drawImage(topImage, 0, 0);
    dst->endFrame();
    dst->flush();

    Pixmap* pm = dst->peekPixels();
    ASSERT_NE(pm, nullptr);

    // Transparent top should not affect the red bottom
    u32 expected = packBGRA(red);
    for (i32 y = 0; y < H; ++y) {
        for (i32 x = 0; x < W; ++x) {
            EXPECT_EQ(readPixel(pm, x, y), expected)
                << "Mismatch at (" << x << ", " << y << ")";
        }
    }
}

// --- Composite three layers: correct stacking order ---

TEST(Composite, ThreeLayersCorrectStackingOrder) {
    // Use a 4x4 surface. Each layer covers a different horizontal band.
    // Layer 1 (bottom): red, rows 0-3
    // Layer 2 (middle): green, rows 1-3
    // Layer 3 (top):    blue, rows 2-3
    // Expected:
    //   row 0: red
    //   row 1: green (overwrites red)
    //   row 2-3: blue (overwrites green)
    const i32 W = 4, H = 4;

    Color red{255, 0, 0, 255};
    Color green{0, 255, 0, 255};
    Color blue{0, 0, 255, 255};

    auto redImage = makeSolidImage(W, H, red);       // 4x4
    auto greenImage = makeSolidImage(W, 3, green);    // 4x3
    auto blueImage = makeSolidImage(W, 2, blue);      // 4x2

    auto dst = Surface::MakeRaster(W, H);
    dst->beginFrame();
    dst->canvas()->drawImage(redImage, 0, 0);     // covers rows 0-3
    dst->canvas()->drawImage(greenImage, 0, 1);   // covers rows 1-3
    dst->canvas()->drawImage(blueImage, 0, 2);    // covers rows 2-3
    dst->endFrame();
    dst->flush();

    Pixmap* pm = dst->peekPixels();
    ASSERT_NE(pm, nullptr);

    u32 redPx = packBGRA(red);
    u32 greenPx = packBGRA(green);
    u32 bluePx = packBGRA(blue);

    for (i32 x = 0; x < W; ++x) {
        EXPECT_EQ(readPixel(pm, x, 0), redPx)   << "Row 0, col " << x;
        EXPECT_EQ(readPixel(pm, x, 1), greenPx) << "Row 1, col " << x;
        EXPECT_EQ(readPixel(pm, x, 2), bluePx)  << "Row 2, col " << x;
        EXPECT_EQ(readPixel(pm, x, 3), bluePx)  << "Row 3, col " << x;
    }
}

// --- Draw image with clip: only clipped region is affected ---

TEST(Composite, DrawImageWithClip) {
    const i32 W = 8, H = 8;
    Color red{255, 0, 0, 255};

    // Full-size red image
    auto image = makeSolidImage(W, H, red);
    ASSERT_NE(image, nullptr);

    auto dst = Surface::MakeRaster(W, H);
    dst->beginFrame();

    // Clip to the top-left 4x4 quadrant
    dst->canvas()->save();
    dst->canvas()->clipRect({0, 0, 4, 4});
    dst->canvas()->drawImage(image, 0, 0);
    dst->canvas()->restore();

    dst->endFrame();
    dst->flush();

    Pixmap* pm = dst->peekPixels();
    ASSERT_NE(pm, nullptr);

    u32 redPx = packBGRA(red);
    u32 blackPx = packBGRA(kBlack);

    for (i32 y = 0; y < H; ++y) {
        for (i32 x = 0; x < W; ++x) {
            bool inClip = (x < 4 && y < 4);
            u32 expected = inClip ? redPx : blackPx;
            EXPECT_EQ(readPixel(pm, x, y), expected)
                << "Mismatch at (" << x << ", " << y << ")";
        }
    }
}

// --- makeSnapshot creates independent copy ---

TEST(Composite, SnapshotIsIndependentCopy) {
    const i32 W = 4, H = 4;
    Color red{255, 0, 0, 255};
    Color green{0, 255, 0, 255};

    // Create a surface, fill with red, take snapshot
    auto src = Surface::MakeRaster(W, H);
    src->beginFrame();
    src->canvas()->fillRect({0, 0, f32(W), f32(H)}, red);
    src->endFrame();
    src->flush();

    auto snapshot = src->makeSnapshot();
    ASSERT_NE(snapshot, nullptr);

    // Now overwrite the source surface with green
    src->beginFrame();
    src->canvas()->fillRect({0, 0, f32(W), f32(H)}, green);
    src->endFrame();
    src->flush();

    // Verify the source is now green
    Pixmap* srcPm = src->peekPixels();
    ASSERT_NE(srcPm, nullptr);
    EXPECT_EQ(readPixel(srcPm, 0, 0), packBGRA(green));

    // Verify the snapshot still contains red (independent copy)
    ASSERT_TRUE(snapshot->valid());
    const u32* snapPixels = snapshot->pixels32();
    ASSERT_NE(snapPixels, nullptr);

    // Read pixel (0,0) from the snapshot - it's stored in BGRA8888 format
    u32 snapPx = snapPixels[0];
    EXPECT_EQ(snapPx, packBGRA(red))
        << "Snapshot should still contain red after source was modified";
}
