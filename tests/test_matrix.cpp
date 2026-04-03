#include <gtest/gtest.h>
#include <ink/matrix.hpp>
#include <cmath>

using namespace ink;

static constexpr f32 kEps = 1e-5f;

static void expectPointNear(Point actual, Point expected, f32 eps = kEps) {
    EXPECT_NEAR(actual.x, expected.x, eps);
    EXPECT_NEAR(actual.y, expected.y, eps);
}

// --- Identity ---

TEST(Matrix, IdentityIsDefault) {
    Matrix m;
    EXPECT_TRUE(m.isIdentity());
    EXPECT_TRUE(m.isTranslateOnly());
    EXPECT_TRUE(m.isScaleTranslateOnly());
}

TEST(Matrix, IdentityMapPointUnchanged) {
    auto p = Matrix::Identity().mapPoint({5, 10});
    EXPECT_FLOAT_EQ(p.x, 5);
    EXPECT_FLOAT_EQ(p.y, 10);
}

// --- Translate ---

TEST(Matrix, TranslateMapPoint) {
    auto p = Matrix::Translate(10, 20).mapPoint({5, 5});
    EXPECT_FLOAT_EQ(p.x, 15);
    EXPECT_FLOAT_EQ(p.y, 25);
}

TEST(Matrix, TranslateIsTranslateOnly) {
    auto m = Matrix::Translate(3, 4);
    EXPECT_FALSE(m.isIdentity());
    EXPECT_TRUE(m.isTranslateOnly());
    EXPECT_TRUE(m.isScaleTranslateOnly());
}

// --- Scale ---

TEST(Matrix, ScaleMapPoint) {
    auto p = Matrix::Scale(2, 3).mapPoint({4, 5});
    EXPECT_FLOAT_EQ(p.x, 8);
    EXPECT_FLOAT_EQ(p.y, 15);
}

TEST(Matrix, ScaleIsScaleTranslateOnly) {
    auto m = Matrix::Scale(2, 3);
    EXPECT_FALSE(m.isIdentity());
    EXPECT_FALSE(m.isTranslateOnly());
    EXPECT_TRUE(m.isScaleTranslateOnly());
}

// --- Rotate ---

TEST(Matrix, Rotate90DegreesMapPoint) {
    auto m = Matrix::Rotate(f32(M_PI / 2));
    auto p = m.mapPoint({1, 0});
    expectPointNear(p, {0, 1});
}

TEST(Matrix, Rotate180DegreesMapPoint) {
    auto m = Matrix::Rotate(f32(M_PI));
    auto p = m.mapPoint({1, 0});
    expectPointNear(p, {-1, 0});
}

TEST(Matrix, RotateIsNotScaleTranslateOnly) {
    auto m = Matrix::Rotate(0.5f);
    EXPECT_FALSE(m.isIdentity());
    EXPECT_FALSE(m.isTranslateOnly());
    EXPECT_FALSE(m.isScaleTranslateOnly());
}

// --- RotateAround ---

TEST(Matrix, RotateAround90) {
    auto m = Matrix::RotateAround(f32(M_PI / 2), 5, 5);
    auto p = m.mapPoint({10, 5});
    expectPointNear(p, {5, 10});
}

// --- Concatenation ---

TEST(Matrix, ConcatTranslateScale) {
    // Translate(10,0) * Scale(2,1) means: scale first, then translate
    auto m = Matrix::Translate(10, 0) * Matrix::Scale(2, 1);
    auto p = m.mapPoint({5, 0});
    EXPECT_FLOAT_EQ(p.x, 20);  // 5 * 2 + 10
    EXPECT_FLOAT_EQ(p.y, 0);
}

TEST(Matrix, ConcatScaleTranslate) {
    // Scale(2,1) * Translate(10,0) means: translate first, then scale
    auto m = Matrix::Scale(2, 1) * Matrix::Translate(10, 0);
    auto p = m.mapPoint({5, 0});
    EXPECT_FLOAT_EQ(p.x, 30);  // (5 + 10) * 2
    EXPECT_FLOAT_EQ(p.y, 0);
}

TEST(Matrix, ConcatAssign) {
    Matrix m = Matrix::Translate(10, 0);
    m *= Matrix::Scale(2, 1);
    auto p = m.mapPoint({5, 0});
    EXPECT_FLOAT_EQ(p.x, 20);
}

// --- mapRect ---

TEST(Matrix, MapRectIdentity) {
    Rect r = Matrix::Identity().mapRect({10, 20, 30, 40});
    EXPECT_FLOAT_EQ(r.x, 10);
    EXPECT_FLOAT_EQ(r.y, 20);
    EXPECT_FLOAT_EQ(r.w, 30);
    EXPECT_FLOAT_EQ(r.h, 40);
}

TEST(Matrix, MapRectTranslate) {
    Rect r = Matrix::Translate(5, 10).mapRect({0, 0, 20, 30});
    EXPECT_FLOAT_EQ(r.x, 5);
    EXPECT_FLOAT_EQ(r.y, 10);
    EXPECT_FLOAT_EQ(r.w, 20);
    EXPECT_FLOAT_EQ(r.h, 30);
}

TEST(Matrix, MapRectScale) {
    Rect r = Matrix::Scale(2, 3).mapRect({10, 10, 20, 10});
    EXPECT_FLOAT_EQ(r.x, 20);
    EXPECT_FLOAT_EQ(r.y, 30);
    EXPECT_FLOAT_EQ(r.w, 40);
    EXPECT_FLOAT_EQ(r.h, 30);
}

TEST(Matrix, MapRectRotation) {
    // 90-degree rotation of a 10x20 rect at origin produces a 20x10 bounding box
    Rect r = Matrix::Rotate(f32(M_PI / 2)).mapRect({0, 0, 10, 20});
    EXPECT_NEAR(r.w, 20, kEps);
    EXPECT_NEAR(r.h, 10, kEps);
}

// --- Inverse ---

TEST(Matrix, InverseIdentity) {
    bool ok = false;
    auto inv = Matrix::Identity().inverted(&ok);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(inv.isIdentity());
}

TEST(Matrix, InverseTranslate) {
    bool ok = false;
    auto inv = Matrix::Translate(10, 20).inverted(&ok);
    EXPECT_TRUE(ok);
    auto p = inv.mapPoint({10, 20});
    expectPointNear(p, {0, 0});
}

TEST(Matrix, InverseScale) {
    bool ok = false;
    auto inv = Matrix::Scale(2, 4).inverted(&ok);
    EXPECT_TRUE(ok);
    auto p = inv.mapPoint({8, 20});
    expectPointNear(p, {4, 5});
}

TEST(Matrix, InverseRotate) {
    auto m = Matrix::Rotate(0.7f);
    bool ok = false;
    auto inv = m.inverted(&ok);
    EXPECT_TRUE(ok);
    auto result = (m * inv);
    EXPECT_NEAR(result.scaleX, 1, kEps);
    EXPECT_NEAR(result.skewX, 0, kEps);
    EXPECT_NEAR(result.skewY, 0, kEps);
    EXPECT_NEAR(result.scaleY, 1, kEps);
    EXPECT_NEAR(result.transX, 0, kEps);
    EXPECT_NEAR(result.transY, 0, kEps);
}

TEST(Matrix, InverseNonInvertible) {
    Matrix m;
    m.scaleX = 0; m.skewX = 0;
    m.skewY = 0;  m.scaleY = 0;
    m.transX = 5; m.transY = 5;
    bool ok = true;
    auto inv = m.inverted(&ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(inv.isIdentity());
}

TEST(Matrix, InverseComplex) {
    auto m = Matrix::Translate(10, 20) * Matrix::Rotate(1.2f) * Matrix::Scale(3, 0.5f);
    bool ok = false;
    auto inv = m.inverted(&ok);
    EXPECT_TRUE(ok);
    // m * inv should be identity
    auto id = m * inv;
    EXPECT_NEAR(id.scaleX, 1, kEps);
    EXPECT_NEAR(id.scaleY, 1, kEps);
    EXPECT_NEAR(id.skewX, 0, kEps);
    EXPECT_NEAR(id.skewY, 0, kEps);
    EXPECT_NEAR(id.transX, 0, kEps);
    EXPECT_NEAR(id.transY, 0, kEps);
}

// --- Equality ---

TEST(Matrix, Equality) {
    EXPECT_EQ(Matrix::Identity(), Matrix::Identity());
    EXPECT_NE(Matrix::Translate(1, 0), Matrix::Identity());
    EXPECT_EQ(Matrix::Scale(2, 3), Matrix::Scale(2, 3));
}
