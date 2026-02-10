#include <gtest/gtest.h>
#include <ink/ink.hpp>
#include <cstring>

using namespace ink;

// --- MakeFromPixmap ---

TEST(Image, MakeFromPixmapValidReturnsNonNull) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(8, 8));
    ASSERT_TRUE(pm.valid());

    auto img = Image::MakeFromPixmap(pm);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->width(), 8);
    EXPECT_EQ(img->height(), 8);
}

TEST(Image, MakeFromPixmapCopiesData) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(4, 4));
    ASSERT_TRUE(pm.valid());

    // Fill source with a known pattern
    u32* src = pm.addr32();
    for (i32 i = 0; i < 4 * 4; ++i) {
        src[i] = 0xAABBCCDD;
    }

    auto img = Image::MakeFromPixmap(pm);
    ASSERT_NE(img, nullptr);

    // Verify the image captured the data
    const u32* imgPixels = img->pixels32();
    EXPECT_EQ(imgPixels[0], 0xAABBCCDD);

    // Modify the original pixmap
    src[0] = 0x11223344;

    // Image data must be unaffected (it was copied)
    EXPECT_EQ(imgPixels[0], 0xAABBCCDD);
}

TEST(Image, MakeFromPixmapInvalidReturnsNull) {
    Pixmap pm;  // default-constructed, invalid
    EXPECT_FALSE(pm.valid());

    auto img = Image::MakeFromPixmap(pm);
    EXPECT_EQ(img, nullptr);
}

TEST(Image, MakeFromPixmapZeroDimensionsReturnsNull) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(0, 0));
    auto img = Image::MakeFromPixmap(pm);
    EXPECT_EQ(img, nullptr);
}

// --- MakeFromPixmapNoCopy ---

TEST(Image, MakeFromPixmapNoCopyReturnsNonNull) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(16, 16));
    ASSERT_TRUE(pm.valid());

    auto img = Image::MakeFromPixmapNoCopy(pm);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->width(), 16);
    EXPECT_EQ(img->height(), 16);
}

TEST(Image, MakeFromPixmapNoCopySharesData) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(4, 4));
    ASSERT_TRUE(pm.valid());

    auto img = Image::MakeFromPixmapNoCopy(pm);
    ASSERT_NE(img, nullptr);

    // The image should point to the same pixel buffer
    EXPECT_EQ(img->pixels(), pm.addr());
}

TEST(Image, MakeFromPixmapNoCopyInvalidReturnsNull) {
    Pixmap pm;
    auto img = Image::MakeFromPixmapNoCopy(pm);
    EXPECT_EQ(img, nullptr);
}

// --- valid ---

TEST(Image, ValidReturnsTrueForValidImage) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(2, 2));
    ASSERT_TRUE(pm.valid());

    auto img = Image::MakeFromPixmap(pm);
    ASSERT_NE(img, nullptr);
    EXPECT_TRUE(img->valid());
}

// --- pixels32 ---

TEST(Image, Pixels32ReturnsNonNull) {
    auto pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(4, 4));
    ASSERT_TRUE(pm.valid());

    auto img = Image::MakeFromPixmap(pm);
    ASSERT_NE(img, nullptr);
    EXPECT_NE(img->pixels32(), nullptr);
}

// --- stride ---

TEST(Image, StrideMatchesSource) {
    auto info = PixmapInfo::MakeRGBA(10, 5);
    auto pm = Pixmap::Alloc(info);
    ASSERT_TRUE(pm.valid());

    auto img = Image::MakeFromPixmap(pm);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->stride(), pm.stride());
}

TEST(Image, StrideMatchesSourceBGRA) {
    auto info = PixmapInfo::MakeBGRA(12, 8);
    auto pm = Pixmap::Alloc(info);
    ASSERT_TRUE(pm.valid());

    auto img = Image::MakeFromPixmap(pm);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->stride(), pm.stride());
    EXPECT_EQ(img->format(), PixelFormat::BGRA8888);
}

// --- MakeFromGLTexture / MakeFromBackendTexture ---

TEST(Image, MakeFromGLTextureValidReturnsNonNull) {
    auto img = Image::MakeFromGLTexture(42, 32, 16, PixelFormat::RGBA8888);
    ASSERT_NE(img, nullptr);
    EXPECT_TRUE(img->valid());
    EXPECT_TRUE(img->isGpuBacked());
    EXPECT_FALSE(img->isCpuBacked());
    EXPECT_EQ(img->glTextureId(), 42u);
    EXPECT_EQ(img->backendTextureHandle(), 42u);
    EXPECT_EQ(img->width(), 32);
    EXPECT_EQ(img->height(), 16);
    EXPECT_EQ(img->pixels(), nullptr);
}

TEST(Image, MakeFromGLTextureInvalidReturnsNull) {
    EXPECT_EQ(Image::MakeFromGLTexture(0, 32, 16), nullptr);
    EXPECT_EQ(Image::MakeFromGLTexture(11, 0, 16), nullptr);
    EXPECT_EQ(Image::MakeFromGLTexture(11, 32, 0), nullptr);
}

TEST(Image, MakeFromBackendTextureValidReturnsNonNull) {
    auto img = Image::MakeFromBackendTexture(99, 64, 32, PixelFormat::RGBA8888);
    ASSERT_NE(img, nullptr);
    EXPECT_TRUE(img->valid());
    EXPECT_TRUE(img->isGpuBacked());
    EXPECT_EQ(img->backendTextureHandle(), 99u);
    EXPECT_EQ(img->glTextureId(), 99u);  // convenience accessor
    EXPECT_EQ(img->width(), 64);
    EXPECT_EQ(img->height(), 32);
}

TEST(Image, MakeFromBackendTextureInvalidReturnsNull) {
    EXPECT_EQ(Image::MakeFromBackendTexture(0, 32, 16), nullptr);
    EXPECT_EQ(Image::MakeFromBackendTexture(11, 0, 16), nullptr);
    EXPECT_EQ(Image::MakeFromBackendTexture(11, 32, 0), nullptr);
}
