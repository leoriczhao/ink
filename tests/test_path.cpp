#include "../src/cpu_renderer.hpp"
#include <cmath>
#include <gtest/gtest.h>
#include <ink/draw_op_visitor.hpp>
#include <ink/draw_pass.hpp>
#include <ink/paint.hpp>
#include <ink/path.hpp>
#include <ink/path_effect.hpp>
#include <ink/pixmap.hpp>
#include <ink/recording.hpp>

using namespace ink;

// --- Path construction ---

TEST(Path, DefaultEmpty) {
  Path p;
  EXPECT_TRUE(p.isEmpty());
  EXPECT_EQ(p.countVerbs(), 0);
  EXPECT_EQ(p.countPoints(), 0);
}

TEST(Path, MoveTo) {
  Path p;
  p.moveTo(10, 20);
  EXPECT_FALSE(p.isEmpty());
  EXPECT_EQ(p.countVerbs(), 1);
  EXPECT_EQ(p.countPoints(), 1);
  EXPECT_EQ(p.verbs()[0], Path::Verb::Move);
  EXPECT_FLOAT_EQ(p.points()[0].x, 10.0f);
  EXPECT_FLOAT_EQ(p.points()[0].y, 20.0f);
}

TEST(Path, LineToAutoMoveTo) {
  Path p;
  p.lineTo(50, 60);
  EXPECT_EQ(p.countVerbs(), 2);
  EXPECT_EQ(p.verbs()[0], Path::Verb::Move);
  EXPECT_EQ(p.verbs()[1], Path::Verb::Line);
}

TEST(Path, QuadTo) {
  Path p;
  p.moveTo(0, 0).quadTo(50, 100, 100, 0);
  EXPECT_EQ(p.countVerbs(), 2);
  EXPECT_EQ(p.countPoints(), 3);
}

TEST(Path, CubicTo) {
  Path p;
  p.moveTo(0, 0).cubicTo(25, 100, 75, 100, 100, 0);
  EXPECT_EQ(p.countVerbs(), 2);
  EXPECT_EQ(p.countPoints(), 4);
}

TEST(Path, Close) {
  Path p;
  p.moveTo(0, 0).lineTo(100, 0).lineTo(100, 100).close();
  EXPECT_EQ(p.countVerbs(), 4);
  EXPECT_EQ(p.verbs().back(), Path::Verb::Close);
}

TEST(Path, Reset) {
  Path p;
  p.moveTo(0, 0).lineTo(100, 100).close();
  p.reset();
  EXPECT_TRUE(p.isEmpty());
  EXPECT_EQ(p.countVerbs(), 0);
}

// --- Convenience builders ---

TEST(Path, AddRect) {
  Path p;
  p.addRect({10, 20, 100, 80});
  EXPECT_FALSE(p.isEmpty());
  EXPECT_GE(p.countVerbs(), 5);
  EXPECT_EQ(p.verbs().back(), Path::Verb::Close);
}

TEST(Path, AddCircle) {
  Path p;
  p.addCircle(50, 50, 25);
  EXPECT_FALSE(p.isEmpty());
  EXPECT_EQ(p.verbs().back(), Path::Verb::Close);
}

TEST(Path, AddRoundRect) {
  Path p;
  p.addRoundRect({0, 0, 100, 80}, 10, 10);
  EXPECT_FALSE(p.isEmpty());
  EXPECT_EQ(p.verbs().back(), Path::Verb::Close);
}

TEST(Path, AddPoly) {
  Point pts[] = {{0, 0}, {100, 0}, {50, 100}};
  Path p;
  p.addPoly(pts, 3, true);
  EXPECT_EQ(p.countVerbs(), 4);
  EXPECT_EQ(p.verbs().back(), Path::Verb::Close);
}

// --- Bounds ---

TEST(Path, BoundsTriangle) {
  Path p;
  p.moveTo(10, 20).lineTo(90, 20).lineTo(50, 80).close();
  Rect b = p.bounds();
  EXPECT_FLOAT_EQ(b.x, 10.0f);
  EXPECT_FLOAT_EQ(b.y, 20.0f);
  EXPECT_FLOAT_EQ(b.w, 80.0f);
  EXPECT_FLOAT_EQ(b.h, 60.0f);
}

TEST(Path, ContainsPointInRect) {
  Path p;
  p.addRect({10, 10, 40, 30});
  EXPECT_TRUE(p.contains({20, 20}));
  EXPECT_FALSE(p.contains({5, 20}));
  EXPECT_FALSE(p.contains({20, 45}));
}

TEST(Path, ContainsUsesEvenOddFillRule) {
  Path p;
  p.addRect({0, 0, 100, 100});
  p.addRect({25, 25, 50, 50});
  EXPECT_TRUE(p.contains({10, 10}));
  EXPECT_TRUE(p.contains({50, 50}));

  p.setFillRule(FillRule::EvenOdd);
  EXPECT_TRUE(p.contains({10, 10}));
  EXPECT_FALSE(p.contains({50, 50}));
}

TEST(Path, DirectionMatchesBuilderDirection) {
  Path cw;
  cw.addRect({0, 0, 10, 10}, PathDirection::CW);
  PathDirection dir = PathDirection::CCW;
  ASSERT_TRUE(cw.getDirection(&dir));
  EXPECT_EQ(dir, PathDirection::CW);

  Path ccw;
  ccw.addRect({0, 0, 10, 10}, PathDirection::CCW);
  ASSERT_TRUE(ccw.getDirection(&dir));
  EXPECT_EQ(dir, PathDirection::CCW);
}

TEST(Path, ConcavePolygonIsNotConvex) {
  Point pts[] = {{0, 0}, {100, 0}, {50, 50}, {100, 100}, {0, 100}};
  Path p;
  p.addPoly(pts, 5, true);
  EXPECT_FALSE(p.isConvex());
}

// --- Transform ---

TEST(Path, TransformTranslate) {
  Path p;
  p.moveTo(0, 0).lineTo(100, 0);
  p.transform(Matrix::Translate(10, 20));
  EXPECT_FLOAT_EQ(p.points()[0].x, 10.0f);
  EXPECT_FLOAT_EQ(p.points()[0].y, 20.0f);
  EXPECT_FLOAT_EQ(p.points()[1].x, 110.0f);
  EXPECT_FLOAT_EQ(p.points()[1].y, 20.0f);
}

TEST(Path, TransformedReturnsNewPath) {
  Path p;
  p.moveTo(0, 0).lineTo(100, 0);
  Path p2 = p.transformed(Matrix::Scale(2, 3));
  EXPECT_FLOAT_EQ(p.points()[1].x, 100.0f);
  EXPECT_FLOAT_EQ(p2.points()[1].x, 200.0f);
  EXPECT_FLOAT_EQ(p2.points()[1].y, 0.0f);
}

// --- Fill Rule ---

TEST(Path, FillRuleDefault) {
  Path p;
  EXPECT_EQ(p.fillRule(), FillRule::Winding);
}

TEST(Path, SetFillRule) {
  Path p;
  p.setFillRule(FillRule::EvenOdd);
  EXPECT_EQ(p.fillRule(), FillRule::EvenOdd);
}

// --- Recording integration ---

TEST(Path, FillPathRecorded) {
  Recorder rec;
  Path p;
  p.addRect({10, 20, 100, 80});
  Paint paint = Paint::Fill({255, 0, 0, 255});
  rec.fillPath(p, paint);

  auto recording = rec.finish();
  ASSERT_EQ(recording->ops().size(), 1u);
  EXPECT_EQ(recording->ops()[0].type, DrawOp::Type::FillPath);
  ASSERT_EQ(recording->paths().size(), 1u);
  EXPECT_FALSE(recording->paths()[0].isEmpty());
}

TEST(Path, StrokePathRecorded) {
  Recorder rec;
  Path p;
  p.moveTo(0, 0).lineTo(100, 100);
  Paint paint = Paint::Stroke({0, 255, 0, 255}, 3.0f);
  rec.strokePath(p, paint);

  auto recording = rec.finish();
  ASSERT_EQ(recording->ops().size(), 1u);
  EXPECT_EQ(recording->ops()[0].type, DrawOp::Type::StrokePath);
  EXPECT_FLOAT_EQ(recording->ops()[0].width, 3.0f);
}

TEST(PathEffect, DashFiltersLineIntoOnSegments) {
  f32 intervals[] = {10.0f, 10.0f};
  auto effect = PathEffects::MakeDash(intervals, 2);
  ASSERT_NE(effect, nullptr);

  Path line;
  line.moveTo(0, 0).lineTo(35, 0);
  Path dashed = effect->filterPath(line);

  EXPECT_EQ(dashed.countVerbs(), 4);
  ASSERT_EQ(dashed.countPoints(), 4);
  EXPECT_FLOAT_EQ(dashed.points()[0].x, 0.0f);
  EXPECT_FLOAT_EQ(dashed.points()[1].x, 10.0f);
  EXPECT_FLOAT_EQ(dashed.points()[2].x, 20.0f);
  EXPECT_FLOAT_EQ(dashed.points()[3].x, 30.0f);
}

TEST(PathEffect, RecorderStoresDashedStrokePath) {
  f32 intervals[] = {8.0f, 4.0f};
  Paint paint = Paint::Stroke({255, 0, 0, 255}, 2.0f);
  paint.pathEffect = PathEffects::MakeDash(intervals, 2);

  Recorder rec;
  Path line;
  line.moveTo(0, 0).lineTo(24, 0);
  rec.strokePath(line, paint);

  auto recording = rec.finish();
  ASSERT_EQ(recording->paths().size(), 1u);
  EXPECT_GT(recording->paths()[0].countVerbs(), line.countVerbs());
}

// --- CPU rendering ---

TEST(Path, FillPathRendersCenterPixel) {
  Pixmap pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(100, 100));
  CpuRenderer renderer(&pm);
  renderer.beginFrame({0, 0, 0, 255});

  Recorder rec;
  Path p;
  p.addRect({20, 20, 60, 60});
  Paint paint = Paint::Fill({255, 0, 0, 255});
  rec.fillPath(p, paint);

  auto recording = rec.finish();
  auto pass = DrawPass::create(*recording);
  renderer.execute(*recording, pass);

  const u32 *pixels = static_cast<const u32 *>(pm.addr());
  u32 centerPixel = pixels[50 * 100 + 50];
  u8 r = centerPixel & 0xFF;
  EXPECT_EQ(r, 255);

  u32 cornerPixel = pixels[0];
  u8 cornerR = cornerPixel & 0xFF;
  EXPECT_EQ(cornerR, 0);
}

TEST(Path, FillCirclePathRendersCorrectly) {
  Pixmap pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(100, 100));
  CpuRenderer renderer(&pm);
  renderer.beginFrame({0, 0, 0, 255});

  Recorder rec;
  Path p;
  p.addCircle(50, 50, 20);
  Paint paint = Paint::Fill({0, 255, 0, 255});
  rec.fillPath(p, paint);

  auto recording = rec.finish();
  auto pass = DrawPass::create(*recording);
  renderer.execute(*recording, pass);

  const u32 *pixels = static_cast<const u32 *>(pm.addr());
  u32 centerPixel = pixels[50 * 100 + 50];
  u8 g = (centerPixel >> 8) & 0xFF;
  EXPECT_EQ(g, 255);

  u32 cornerPixel = pixels[0];
  u8 cornerG = (cornerPixel >> 8) & 0xFF;
  EXPECT_EQ(cornerG, 0);
}

TEST(Path, StrokePathRendersEdge) {
  Pixmap pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(100, 100));
  CpuRenderer renderer(&pm);
  renderer.beginFrame({0, 0, 0, 255});

  Recorder rec;
  Path p;
  p.moveTo(10, 50).lineTo(90, 50);
  Paint paint = Paint::Stroke({0, 0, 255, 255}, 4.0f);
  paint.antiAlias = false;
  rec.strokePath(p, paint);

  auto recording = rec.finish();
  auto pass = DrawPass::create(*recording);
  renderer.execute(*recording, pass);

  const u32 *pixels = static_cast<const u32 *>(pm.addr());
  u32 linePixel = pixels[50 * 100 + 50];
  u8 b = (linePixel >> 16) & 0xFF;
  EXPECT_GT(b, 0);
}

TEST(Path, ClipPathLimitsCpuRendering) {
  Pixmap pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(100, 100));
  CpuRenderer renderer(&pm);
  renderer.beginFrame({0, 0, 0, 255});

  Recorder rec;
  Path clip;
  clip.addCircle(50, 50, 20);
  rec.setClipPath(clip);
  rec.fillRect({0, 0, 100, 100}, {255, 0, 0, 255});

  auto recording = rec.finish();
  auto pass = DrawPass::create(*recording);
  renderer.execute(*recording, pass);

  const u32 *pixels = static_cast<const u32 *>(pm.addr());
  u8 centerR = pixels[50 * 100 + 50] & 0xFF;
  u8 cornerR = pixels[5 * 100 + 5] & 0xFF;
  EXPECT_EQ(centerR, 255);
  EXPECT_EQ(cornerR, 0);
}
