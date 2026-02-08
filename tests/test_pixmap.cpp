#include <gtest/gtest.h>
#include <ink/pixmap.hpp>
#include <cstring>
#include <vector>

using namespace ink;

// --- PixmapInfo ---

TEST(PixmapInfo, Make) {
    auto info = PixmapInfo::Make(100, 200, PixelFormat::RGBA8888);
    EXPECT_EQ(info.width, 100);
    EXPECT_EQ(info.height, 200);
    EXPECT_EQ(info.stride, 100 * 4);
    EXPECT_EQ(info.format, PixelFormat::RGBA8888);
}

TEST(PixmapInfo, MakeRGBA) {
    auto info = PixmapInfo::MakeRGBA(64, 32);
    EXPECT_EQ(info.width, 64);
    EXPECT_EQ(info.height, 32);
    EXPECT_EQ(info.format, PixelFormat::RGBA8888);
}

TEST(PixmapInfo, MakeBGRA) {
    auto info = PixmapInfo::MakeBGRA(64, 32);
    EXPECT_EQ(info.format, PixelFormat::BGRA8888);
}

TEST(PixmapInfo, ComputeByteSize) {
    auto info = PixmapInfo::Make(10, 20, PixelFormat::RGBA8888);
    // stride = 10 * 4 = 40, byteSize = 40 * 20 = 800
    EXPECT_EQ(info.computeByteSize(), 800);
}

TEST(PixmapInfo, BytesPerPixel) {
    PixmapInfo info;
    EXPECT_EQ(info.bytesPerPixel(), 4);
}

// --- Pixmap::Alloc ---

TEST(Pixmap, AllocCreatesValidPixmap) {
    auto info = PixmapInfo::MakeRGBA(16, 16);
    auto pm = Pixmap::Alloc(info);
    EXPECT_TRUE(pm.valid());
    EXPECT_EQ(pm.width(), 16);
    EXPECT_EQ(pm.height(), 16);
    EXPECT_EQ(pm.stride(), 16 * 4);
    EXPECT_NE(pm.addr(), nullptr);
}

TEST(Pixmap, AllocZeroWidthReturnsInvalid) {
    auto info = PixmapInfo::MakeRGBA(0, 16);
    auto pm = Pixmap::Alloc(info);
    EXPECT_FALSE(pm.valid());
}

TEST(Pixmap, AllocZeroHeightReturnsInvalid) {
    auto info = PixmapInfo::MakeRGBA(16, 0);
    auto pm = Pixmap::Alloc(info);
    EXPECT_FALSE(pm.valid());
}

TEST(Pixmap, AllocZeroBothReturnsInvalid) {
    auto info = PixmapInfo::MakeRGBA(0, 0);
    auto pm = Pixmap::Alloc(info);
    EXPECT_FALSE(pm.valid());
}

// --- Pixmap::Wrap ---

TEST(Pixmap, WrapExternalMemory) {
    auto info = PixmapInfo::MakeRGBA(4, 4);
    std::vector<u8> buffer(info.computeByteSize(), 0);
    auto pm = Pixmap::Wrap(info, buffer.data());
    EXPECT_TRUE(pm.valid());
    EXPECT_EQ(pm.addr(), buffer.data());
    EXPECT_EQ(pm.width(), 4);
    EXPECT_EQ(pm.height(), 4);
}

// --- Pixmap::clear ---

TEST(Pixmap, ClearSetsAllPixels) {
    auto info = PixmapInfo::MakeRGBA(4, 4);
    auto pm = Pixmap::Alloc(info);
    ASSERT_TRUE(pm.valid());

    Color red{255, 0, 0, 255};
    pm.clear(red);

    const u32* pixels = pm.addr32();
    i32 totalPixels = pm.width() * pm.height();
    u32 expected;
    std::memcpy(&expected, &red, sizeof(u32));

    for (i32 i = 0; i < totalPixels; ++i) {
        EXPECT_EQ(pixels[i], expected) << "Mismatch at pixel " << i;
    }
}

// --- rowAddr ---

TEST(Pixmap, RowAddrReturnsCorrectPointer) {
    auto info = PixmapInfo::MakeRGBA(8, 4);
    auto pm = Pixmap::Alloc(info);
    ASSERT_TRUE(pm.valid());

    for (i32 y = 0; y < pm.height(); ++y) {
        const void* expected = static_cast<const u8*>(pm.addr()) + y * pm.stride();
        EXPECT_EQ(pm.rowAddr(y), expected) << "Row " << y;
    }
}

// --- addr32 ---

TEST(Pixmap, Addr32ReturnsNonNullForValid) {
    auto info = PixmapInfo::MakeRGBA(2, 2);
    auto pm = Pixmap::Alloc(info);
    ASSERT_TRUE(pm.valid());
    EXPECT_NE(pm.addr32(), nullptr);
}

TEST(Pixmap, Addr32ReturnsNullForDefault) {
    Pixmap pm;
    EXPECT_EQ(pm.addr32(), nullptr);
}

// --- format ---

TEST(Pixmap, FormatReturnsCorrectFormat) {
    auto rgba = Pixmap::Alloc(PixmapInfo::MakeRGBA(2, 2));
    EXPECT_EQ(rgba.format(), PixelFormat::RGBA8888);

    auto bgra = Pixmap::Alloc(PixmapInfo::MakeBGRA(2, 2));
    EXPECT_EQ(bgra.format(), PixelFormat::BGRA8888);
}

// --- reallocate ---

TEST(Pixmap, ReallocateChangesDimensions) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(4, 4));
    ASSERT_TRUE(pm.valid());
    EXPECT_EQ(pm.width(), 4);
    EXPECT_EQ(pm.height(), 4);

    auto newInfo = PixmapInfo::MakeRGBA(8, 16);
    pm.reallocate(newInfo);
    EXPECT_TRUE(pm.valid());
    EXPECT_EQ(pm.width(), 8);
    EXPECT_EQ(pm.height(), 16);
    EXPECT_EQ(pm.stride(), 8 * 4);
}

// --- Move semantics ---

TEST(Pixmap, MoveConstructor) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(4, 4));
    ASSERT_TRUE(pm.valid());
    void* originalAddr = pm.addr();

    Pixmap moved(std::move(pm));
    EXPECT_TRUE(moved.valid());
    EXPECT_EQ(moved.addr(), originalAddr);
    EXPECT_FALSE(pm.valid());
}

TEST(Pixmap, MoveAssignment) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(4, 4));
    ASSERT_TRUE(pm.valid());
    void* originalAddr = pm.addr();

    Pixmap moved;
    moved = std::move(pm);
    EXPECT_TRUE(moved.valid());
    EXPECT_EQ(moved.addr(), originalAddr);
    EXPECT_FALSE(pm.valid());
}
