#include <gtest/gtest.h>
#include <ink/types.hpp>

using namespace ink;

// --- Size type aliases ---

TEST(TypeAliases, SizeTypes) {
    static_assert(sizeof(i32) == 4, "i32 must be 4 bytes");
    static_assert(sizeof(u32) == 4, "u32 must be 4 bytes");
    static_assert(sizeof(u64) == 8, "u64 must be 8 bytes");
    static_assert(sizeof(u16) == 2, "u16 must be 2 bytes");
    static_assert(sizeof(u8)  == 1, "u8 must be 1 byte");
    static_assert(sizeof(f32) == 4, "f32 must be 4 bytes");
    static_assert(sizeof(f64) == 8, "f64 must be 8 bytes");

    i32 si = -1;
    u32 ui = 1;
    f32 fi = 1.5f;
    u8  bi = 255;
    EXPECT_EQ(si, -1);
    EXPECT_EQ(ui, 1u);
    EXPECT_FLOAT_EQ(fi, 1.5f);
    EXPECT_EQ(bi, 255);
}

// --- Point ---

TEST(Point, DefaultConstruction) {
    Point p;
    EXPECT_FLOAT_EQ(p.x, 0.0f);
    EXPECT_FLOAT_EQ(p.y, 0.0f);
}

TEST(Point, ValueConstruction) {
    Point p{3.5f, -7.25f};
    EXPECT_FLOAT_EQ(p.x, 3.5f);
    EXPECT_FLOAT_EQ(p.y, -7.25f);
}

TEST(Point, Equality) {
    Point a{1.0f, 2.0f};
    Point b{1.0f, 2.0f};
    Point c{1.0f, 3.0f};
    EXPECT_FLOAT_EQ(a.x, b.x);
    EXPECT_FLOAT_EQ(a.y, b.y);
    EXPECT_NE(a.y, c.y);
}

// --- Rect ---

TEST(Rect, DefaultConstruction) {
    Rect r;
    EXPECT_FLOAT_EQ(r.x, 0.0f);
    EXPECT_FLOAT_EQ(r.y, 0.0f);
    EXPECT_FLOAT_EQ(r.w, 0.0f);
    EXPECT_FLOAT_EQ(r.h, 0.0f);
}

TEST(Rect, ValueConstruction) {
    Rect r{10.0f, 20.0f, 100.0f, 200.0f};
    EXPECT_FLOAT_EQ(r.x, 10.0f);
    EXPECT_FLOAT_EQ(r.y, 20.0f);
    EXPECT_FLOAT_EQ(r.w, 100.0f);
    EXPECT_FLOAT_EQ(r.h, 200.0f);
}

TEST(Rect, Fields) {
    Rect r;
    r.x = 5.0f;
    r.y = 10.0f;
    r.w = 50.0f;
    r.h = 60.0f;
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 10.0f);
    EXPECT_FLOAT_EQ(r.w, 50.0f);
    EXPECT_FLOAT_EQ(r.h, 60.0f);
}

// --- Color ---

TEST(Color, DefaultConstruction) {
    Color c;
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}

TEST(Color, ValueConstruction) {
    Color c{128, 64, 32, 200};
    EXPECT_EQ(c.r, 128);
    EXPECT_EQ(c.g, 64);
    EXPECT_EQ(c.b, 32);
    EXPECT_EQ(c.a, 200);
}

TEST(Color, RGBAFields) {
    Color c;
    c.r = 10;
    c.g = 20;
    c.b = 30;
    c.a = 40;
    EXPECT_EQ(c.r, 10);
    EXPECT_EQ(c.g, 20);
    EXPECT_EQ(c.b, 30);
    EXPECT_EQ(c.a, 40);
}
