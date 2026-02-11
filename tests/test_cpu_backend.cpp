#include <gtest/gtest.h>
#include <ink/cpu_backend.hpp>
#include <ink/draw_pass.hpp>
#include <cstring>

using namespace ink;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Use BGRA8888 so pixel layout in u32 is: 0xAARRGGBB
// (see CpuBackend::blendPixel — for BGRA format it writes
//  0xFF000000 | (r<<16) | (g<<8) | b)
static constexpr PixelFormat kFmt = PixelFormat::BGRA8888;

// Pack an opaque BGRA pixel the same way CpuBackend does.
static u32 packBGRA(Color c) {
    return (u32(c.a) << 24) | (u32(c.r) << 16) | (u32(c.g) << 8) | u32(c.b);
}

// Read a pixel from a pixmap at (x, y).
static u32 readPixel(const Pixmap& pm, i32 x, i32 y) {
    const u32* row = static_cast<const u32*>(pm.rowAddr(y));
    return row[x];
}

// Record ops via a Recorder, build a Recording + DrawPass, execute on
// CpuBackend, then return.  The pixmap is cleared by beginFrame().
static void executeOps(Pixmap& pm, std::function<void(Recorder&)> fn) {
    Recorder rec;
    rec.reset();
    fn(rec);
    auto recording = rec.finish();
    ASSERT_NE(recording, nullptr);

    DrawPass pass = DrawPass::create(*recording);
    CpuBackend backend(&pm);
    backend.beginFrame();  // clears to black
    backend.execute(*recording, pass);
    backend.endFrame();
}

static const Color kBlack{0, 0, 0, 255};
static const Color kRed{255, 0, 0, 255};
static const Color kGreen{0, 255, 0, 255};
static const Color kBlue{0, 0, 255, 255};
static const Color kWhite{255, 255, 255, 255};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(CpuBackend, FillRectWritesCorrectColor) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeBGRA(16, 16));
    ASSERT_TRUE(pm.valid());

    executeOps(pm, [](Recorder& r) {
        r.fillRect({2, 2, 4, 4}, kRed);
    });

    // Pixel inside the rect should be red
    EXPECT_EQ(readPixel(pm, 3, 3), packBGRA(kRed));
    // Pixel outside should be black (cleared by beginFrame)
    EXPECT_EQ(readPixel(pm, 0, 0), packBGRA(kBlack));
}

TEST(CpuBackend, FillRectRespectsClip) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeBGRA(16, 16));
    ASSERT_TRUE(pm.valid());

    executeOps(pm, [](Recorder& r) {
        // Clip to a 4x4 region at (4,4)
        r.setClip({4, 4, 4, 4});
        // Fill a larger rect that extends beyond the clip
        r.fillRect({0, 0, 16, 16}, kGreen);
        r.clearClip();
    });

    // Inside clip: should be green
    EXPECT_EQ(readPixel(pm, 5, 5), packBGRA(kGreen));
    // Outside clip: should remain black
    EXPECT_EQ(readPixel(pm, 0, 0), packBGRA(kBlack));
    EXPECT_EQ(readPixel(pm, 15, 15), packBGRA(kBlack));
}

TEST(CpuBackend, FillRectSemiTransparentBlends) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeBGRA(8, 8));
    ASSERT_TRUE(pm.valid());

    // beginFrame clears to black (0,0,0,255).
    // Then blend 50% white on top.
    Color halfWhite{255, 255, 255, 128};
    executeOps(pm, [&](Recorder& r) {
        r.fillRect({0, 0, 8, 8}, halfWhite);
    });

    u32 pixel = readPixel(pm, 4, 4);
    // Expected: alpha blend of white@128 over black@255
    // outR = (255*128 + 0*(255-128)) / 255 ≈ 128
    u8 outR = (pixel >> 16) & 0xFF;
    u8 outG = (pixel >> 8) & 0xFF;
    u8 outB = pixel & 0xFF;

    // Allow ±1 for integer rounding
    EXPECT_NEAR(outR, 128, 1);
    EXPECT_NEAR(outG, 128, 1);
    EXPECT_NEAR(outB, 128, 1);
}

TEST(CpuBackend, StrokeRectDrawsBorderPixels) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeBGRA(20, 20));
    ASSERT_TRUE(pm.valid());

    executeOps(pm, [](Recorder& r) {
        r.strokeRect({4, 4, 10, 10}, kBlue, 1.0f);
    });

    // Top-left corner of the stroke
    EXPECT_EQ(readPixel(pm, 4, 4), packBGRA(kBlue));
    // Top edge
    EXPECT_EQ(readPixel(pm, 8, 4), packBGRA(kBlue));
    // Left edge
    EXPECT_EQ(readPixel(pm, 4, 8), packBGRA(kBlue));
    // Bottom-right corner
    EXPECT_EQ(readPixel(pm, 14, 14), packBGRA(kBlue));
    // Interior should remain black
    EXPECT_EQ(readPixel(pm, 8, 8), packBGRA(kBlack));
}

TEST(CpuBackend, DrawLineDrawsPixels) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeBGRA(16, 16));
    ASSERT_TRUE(pm.valid());

    executeOps(pm, [](Recorder& r) {
        // Horizontal line from (0,5) to (15,5)
        r.drawLine({0, 5}, {15, 5}, kWhite, 1.0f);
    });

    // Pixels along the line should be white
    EXPECT_EQ(readPixel(pm, 0, 5), packBGRA(kWhite));
    EXPECT_EQ(readPixel(pm, 7, 5), packBGRA(kWhite));
    EXPECT_EQ(readPixel(pm, 15, 5), packBGRA(kWhite));
    // Pixel off the line should be black
    EXPECT_EQ(readPixel(pm, 7, 0), packBGRA(kBlack));
}

TEST(CpuBackend, BeginFrameClearsToBlack) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeBGRA(8, 8));
    ASSERT_TRUE(pm.valid());

    // Dirty the pixmap with white
    pm.clear(kWhite);
    EXPECT_EQ(readPixel(pm, 0, 0), packBGRA(kWhite));

    // beginFrame should clear everything to black
    CpuBackend backend(&pm);
    backend.beginFrame();

    for (i32 y = 0; y < pm.height(); ++y) {
        for (i32 x = 0; x < pm.width(); ++x) {
            EXPECT_EQ(readPixel(pm, x, y), packBGRA(kBlack))
                << "Pixel (" << x << "," << y << ") not cleared";
        }
    }
}

TEST(CpuBackend, SetClipClearClipAffectsDrawing) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeBGRA(16, 16));
    ASSERT_TRUE(pm.valid());

    executeOps(pm, [](Recorder& r) {
        // Set clip to top-left 8x8
        r.setClip({0, 0, 8, 8});
        r.fillRect({0, 0, 16, 16}, kRed);
        r.clearClip();
        // Now fill bottom-right 8x8 with green (no clip)
        r.fillRect({8, 8, 8, 8}, kGreen);
    });

    // Top-left quadrant: red (was inside clip)
    EXPECT_EQ(readPixel(pm, 2, 2), packBGRA(kRed));
    // Bottom-right quadrant: green (drawn after clip cleared)
    EXPECT_EQ(readPixel(pm, 12, 12), packBGRA(kGreen));
    // Top-right quadrant: should be black (outside clip during red fill,
    // not covered by green fill)
    EXPECT_EQ(readPixel(pm, 12, 2), packBGRA(kBlack));
}

TEST(CpuBackend, MultipleFillRectsAllRender) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeBGRA(20, 20));
    ASSERT_TRUE(pm.valid());

    executeOps(pm, [](Recorder& r) {
        r.fillRect({0, 0, 5, 5}, kRed);
        r.fillRect({5, 5, 5, 5}, kGreen);
        r.fillRect({10, 10, 5, 5}, kBlue);
    });

    EXPECT_EQ(readPixel(pm, 2, 2), packBGRA(kRed));
    EXPECT_EQ(readPixel(pm, 7, 7), packBGRA(kGreen));
    EXPECT_EQ(readPixel(pm, 12, 12), packBGRA(kBlue));
    // Untouched area
    EXPECT_EQ(readPixel(pm, 18, 18), packBGRA(kBlack));
}
