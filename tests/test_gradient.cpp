#include "../src/cpu_renderer.hpp"
#include <gtest/gtest.h>
#include <ink/draw_pass.hpp>
#include <ink/paint.hpp>
#include <ink/path.hpp>
#include <ink/pixmap.hpp>
#include <ink/recording.hpp>
#include <ink/shader.hpp>

using namespace ink;

TEST(Gradient, LinearGradientCreation) {
  Color colors[] = {{255, 0, 0, 255}, {0, 0, 255, 255}};
  auto shader = Shaders::MakeLinear({0, 0}, {100, 0}, colors, nullptr, 2);
  ASSERT_NE(shader, nullptr);
}

TEST(Gradient, RadialGradientCreation) {
  Color colors[] = {{255, 255, 0, 255}, {0, 0, 0, 255}};
  auto shader = Shaders::MakeRadial({50, 50}, 40, colors, nullptr, 2);
  ASSERT_NE(shader, nullptr);
}

TEST(Gradient, SweepGradientCreation) {
  Color colors[] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}};
  auto shader = Shaders::MakeSweep({50, 50}, colors, nullptr, 3);
  ASSERT_NE(shader, nullptr);
}

TEST(Gradient, LinearGradientShadeSpan) {
  Color colors[] = {{255, 0, 0, 255}, {0, 0, 255, 255}};
  auto shader = Shaders::MakeLinear({0, 0}, {100, 0}, colors, nullptr, 2);
  ASSERT_NE(shader, nullptr);

  Color out[3];
  shader->shadeSpan(0, 0, 3, out);
  EXPECT_GT(out[0].r, out[2].r);
  EXPECT_LT(out[0].b, out[2].b);
}

TEST(Gradient, LinearGradientInvalidReturnsNull) {
  auto shader = Shaders::MakeLinear({0, 0}, {100, 0}, nullptr, nullptr, 2);
  EXPECT_EQ(shader, nullptr);

  Color colors[] = {{255, 0, 0, 255}};
  auto shader2 = Shaders::MakeLinear({0, 0}, {100, 0}, colors, nullptr, 1);
  EXPECT_EQ(shader2, nullptr);
}

TEST(Gradient, GradientFillPathRendering) {
  Pixmap pm = Pixmap::Alloc(PixmapInfo::MakeRGBA(100, 100));
  CpuRenderer renderer(&pm);
  renderer.beginFrame({0, 0, 0, 255});

  Color colors[] = {{255, 0, 0, 255}, {0, 0, 255, 255}};
  auto shader = Shaders::MakeLinear({0, 0}, {100, 0}, colors, nullptr, 2);

  Recorder rec;
  Path p;
  p.addRect({0, 0, 100, 100});
  Paint paint;
  paint.color = {255, 255, 255, 255};
  paint.shader = shader;
  paint.antiAlias = false;
  rec.fillPath(p, paint);

  auto recording = rec.finish();
  auto pass = DrawPass::create(*recording);
  renderer.execute(*recording, pass);

  const u32 *pixels = static_cast<const u32 *>(pm.addr());
  // Left side should be more red
  u32 leftPixel = pixels[50 * 100 + 5];
  u8 leftR = leftPixel & 0xFF;
  // Right side should be more blue
  u32 rightPixel = pixels[50 * 100 + 95];
  u8 rightB = (rightPixel >> 16) & 0xFF;

  EXPECT_GT(leftR, 100);
  EXPECT_GT(rightB, 100);
}

TEST(Gradient, TileModeRepeat) {
  Color colors[] = {{255, 0, 0, 255}, {0, 255, 0, 255}};
  auto shader = Shaders::MakeLinear({0, 0}, {50, 0}, colors, nullptr, 2,
                                    TileMode::Repeat);
  ASSERT_NE(shader, nullptr);

  Color out[100];
  shader->shadeSpan(0, 0, 100, out);
  // At x=0 and x=50 should be similar (repeat)
  EXPECT_NEAR(out[0].r, out[50].r, 30);
}

TEST(Gradient, RadialGradientCenterColor) {
  Color colors[] = {{255, 0, 0, 255}, {0, 0, 255, 255}};
  auto shader = Shaders::MakeRadial({50, 50}, 40, colors, nullptr, 2);

  Color out[1];
  shader->shadeSpan(50, 50, 1, out);
  EXPECT_GT(out[0].r, 200);
}
