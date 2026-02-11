#include <gtest/gtest.h>
#include <ink/surface.hpp>
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

// --- Factory: MakeRaster ---

TEST(Surface, MakeRasterReturnsNonNull) {
    auto surface = Surface::MakeRaster(16, 16);
    ASSERT_NE(surface, nullptr);
}

TEST(Surface, MakeRasterHasValidCanvas) {
    auto surface = Surface::MakeRaster(16, 16);
    EXPECT_NE(surface->canvas(), nullptr);
}

TEST(Surface, MakeRasterCreatesPixmapWithCorrectDimensions) {
    auto surface = Surface::MakeRaster(32, 64);
    Pixmap* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);
    EXPECT_TRUE(pm->valid());
    EXPECT_EQ(pm->width(), 32);
    EXPECT_EQ(pm->height(), 64);
    EXPECT_EQ(pm->stride(), 32 * 4);
}

// --- Factory: MakeRasterDirect ---

TEST(Surface, MakeRasterDirectWrapsExternalBuffer) {
    const i32 W = 8, H = 8;
    PixmapInfo info = PixmapInfo::Make(W, H, PixelFormat::BGRA8888);
    std::vector<u8> buffer(info.computeByteSize(), 0);

    auto surface = Surface::MakeRasterDirect(info, buffer.data());
    ASSERT_NE(surface, nullptr);

    Pixmap* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);
    EXPECT_EQ(pm->addr(), buffer.data());
    EXPECT_EQ(pm->width(), W);
    EXPECT_EQ(pm->height(), H);
}

// --- Factory: MakeRecording ---

TEST(Surface, MakeRecordingReturnsNonNull) {
    auto surface = Surface::MakeRecording(16, 16);
    ASSERT_NE(surface, nullptr);
}

TEST(Surface, MakeRecordingHasValidCanvas) {
    auto surface = Surface::MakeRecording(16, 16);
    EXPECT_NE(surface->canvas(), nullptr);
}

TEST(Surface, MakeRecordingHasNoPixmap) {
    auto surface = Surface::MakeRecording(16, 16);
    EXPECT_EQ(surface->peekPixels(), nullptr);

    const Surface* cs = surface.get();
    EXPECT_EQ(cs->peekPixels(), nullptr);
}

// --- isGPU ---

TEST(Surface, IsGPUReturnsFalseForRaster) {
    auto surface = Surface::MakeRaster(4, 4);
    EXPECT_FALSE(surface->isGPU());
}

TEST(Surface, IsGPUReturnsFalseForRecording) {
    auto surface = Surface::MakeRecording(4, 4);
    // Recording has no backend and no pixmap, so isGPU() should be false
    EXPECT_FALSE(surface->isGPU());
}

// --- Full lifecycle ---

TEST(Surface, FullLifecycleDoesNotCrash) {
    auto surface = Surface::MakeRaster(10, 10);
    ASSERT_NE(surface, nullptr);

    surface->beginFrame();
    surface->canvas()->fillRect({0, 0, 10, 10}, {255, 0, 0, 255});
    surface->endFrame();
    surface->flush();
    // No crash = pass
}

// --- Flush writes pixels ---

TEST(Surface, FlushWritesPixelsToPixmap) {
    const i32 W = 4, H = 4;
    auto surface = Surface::MakeRaster(W, H);
    ASSERT_NE(surface, nullptr);

    Color red{255, 0, 0, 255};

    surface->beginFrame();
    surface->canvas()->fillRect({0, 0, f32(W), f32(H)}, red);
    surface->endFrame();
    surface->flush();

    Pixmap* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);

    u32 expected = packBGRA(red);
    for (i32 y = 0; y < H; ++y) {
        for (i32 x = 0; x < W; ++x) {
            EXPECT_EQ(readPixel(pm, x, y), expected)
                << "Mismatch at (" << x << ", " << y << ")";
        }
    }
}

// --- resize ---

TEST(Surface, ResizeChangesPixmapDimensions) {
    auto surface = Surface::MakeRaster(8, 8);
    ASSERT_NE(surface, nullptr);

    Pixmap* pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);
    EXPECT_EQ(pm->width(), 8);
    EXPECT_EQ(pm->height(), 8);

    surface->resize(16, 32);

    pm = surface->peekPixels();
    ASSERT_NE(pm, nullptr);
    EXPECT_EQ(pm->width(), 16);
    EXPECT_EQ(pm->height(), 32);
    EXPECT_EQ(pm->stride(), 16 * 4);
}

// --- takeRecording ---

TEST(Surface, TakeRecordingFromRecordingSurface) {
    auto surface = Surface::MakeRecording(10, 10);
    ASSERT_NE(surface, nullptr);

    surface->beginFrame();
    surface->canvas()->fillRect({0, 0, 10, 10}, {0, 255, 0, 255});
    surface->endFrame();

    auto recording = surface->takeRecording();
    ASSERT_NE(recording, nullptr);
    EXPECT_FALSE(recording->ops().empty());
}

// --- getPixelData ---

TEST(Surface, GetPixelDataReturnsValidForRaster) {
    const i32 W = 10, H = 10;
    auto surface = Surface::MakeRaster(W, H);
    ASSERT_NE(surface, nullptr);

    PixelData pd = surface->getPixelData();
    EXPECT_TRUE(pd.isValid());
    EXPECT_EQ(pd.width, W);
    EXPECT_EQ(pd.height, H);
    EXPECT_EQ(pd.rowBytes, W * 4);
    EXPECT_NE(pd.data, nullptr);
    EXPECT_EQ(pd.format, PixelFormat::BGRA8888);
}

TEST(Surface, GetPixelDataReturnsInvalidForRecording) {
    auto surface = Surface::MakeRecording(10, 10);
    PixelData pd = surface->getPixelData();
    EXPECT_FALSE(pd.isValid());
}

// --- makeSnapshot ---

TEST(Surface, MakeSnapshotReturnsNonNullForRaster) {
    auto surface = Surface::MakeRaster(4, 4);
    ASSERT_NE(surface, nullptr);

    // Draw something so the pixmap has valid content
    surface->beginFrame();
    surface->canvas()->fillRect({0, 0, 4, 4}, {128, 64, 32, 255});
    surface->endFrame();
    surface->flush();

    auto image = surface->makeSnapshot();
    ASSERT_NE(image, nullptr);
    EXPECT_TRUE(image->valid());
    EXPECT_EQ(image->width(), 4);
    EXPECT_EQ(image->height(), 4);
}

TEST(Surface, MakeSnapshotReturnsNullForRecording) {
    auto surface = Surface::MakeRecording(4, 4);
    auto image = surface->makeSnapshot();
    EXPECT_EQ(image, nullptr);
}
