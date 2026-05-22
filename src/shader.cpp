#include "ink/shader.hpp"
#include <algorithm>
#include <cmath>

namespace ink {

namespace {

static f32 clampF(f32 v, f32 lo, f32 hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static u8 clampU8(i32 v) {
  return static_cast<u8>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static u8 roundClampU8(f32 v) {
  return clampU8(static_cast<i32>(std::lround(v)));
}

static f32 applyTileMode(f32 t, TileMode mode) {
  switch (mode) {
  case TileMode::Clamp:
    return clampF(t, 0.0f, 1.0f);
  case TileMode::Repeat:
    t = t - std::floor(t);
    return t < 0 ? t + 1.0f : t;
  case TileMode::Mirror: {
    t = t - 2.0f * std::floor(t * 0.5f);
    if (t > 1.0f)
      t = 2.0f - t;
    return clampF(t, 0.0f, 1.0f);
  }
  }
  return clampF(t, 0.0f, 1.0f);
}

static Color lerpColor(Color a, Color b, f32 t) {
  f32 s = 1.0f - t;
  return {
      roundClampU8(a.r * s + b.r * t),
      roundClampU8(a.g * s + b.g * t),
      roundClampU8(a.b * s + b.b * t),
      roundClampU8(a.a * s + b.a * t),
  };
}

struct GradientBase {
  std::vector<Color> colors;
  std::vector<f32> positions;

  void init(const Color *cols, const f32 *pos, i32 count) {
    colors.assign(cols, cols + count);
    if (pos) {
      positions.assign(pos, pos + count);
    } else {
      positions.resize(count);
      for (i32 i = 0; i < count; ++i)
        positions[i] = static_cast<f32>(i) / (count - 1);
    }
  }

  Color evaluate(f32 t) const {
    if (t <= positions.front())
      return colors.front();
    if (t >= positions.back())
      return colors.back();

    for (i32 i = 0; i + 1 < static_cast<i32>(positions.size()); ++i) {
      if (t >= positions[i] && t <= positions[i + 1]) {
        f32 range = positions[i + 1] - positions[i];
        f32 local = (range > 0) ? (t - positions[i]) / range : 0;
        return lerpColor(colors[i], colors[i + 1], local);
      }
    }
    return colors.back();
  }
};

class LinearGradientImpl : public Shader {
public:
  Point start_, end_;
  TileMode mode_;
  GradientBase grad_;
  f32 dx_, dy_, lenSq_;

  LinearGradientImpl(Point start, Point end, const Color *colors,
                     const f32 *positions, i32 count, TileMode mode)
      : start_(start), end_(end), mode_(mode) {
    grad_.init(colors, positions, count);
    dx_ = end.x - start.x;
    dy_ = end.y - start.y;
    lenSq_ = dx_ * dx_ + dy_ * dy_;
    if (lenSq_ < 1e-10f)
      lenSq_ = 1e-10f;
  }

  void shadeSpan(i32 x, i32 y, i32 count, Color *out) const override {
    for (i32 i = 0; i < count; ++i) {
      f32 px = static_cast<f32>(x + i) + 0.5f;
      f32 py = static_cast<f32>(y) + 0.5f;

      if (!localMatrix_.isIdentity()) {
        bool ok;
        Matrix inv = localMatrix_.inverted(&ok);
        if (ok) {
          Point mapped = inv.mapPoint({px, py});
          px = mapped.x;
          py = mapped.y;
        }
      }

      f32 t = ((px - start_.x) * dx_ + (py - start_.y) * dy_) / lenSq_;
      t = applyTileMode(t, mode_);
      out[i] = grad_.evaluate(t);
    }
  }
};

class RadialGradientImpl : public Shader {
public:
  Point center_;
  f32 radius_;
  TileMode mode_;
  GradientBase grad_;

  RadialGradientImpl(Point center, f32 radius, const Color *colors,
                     const f32 *positions, i32 count, TileMode mode)
      : center_(center), radius_(radius), mode_(mode) {
    grad_.init(colors, positions, count);
    if (radius_ < 1e-6f)
      radius_ = 1e-6f;
  }

  void shadeSpan(i32 x, i32 y, i32 count, Color *out) const override {
    for (i32 i = 0; i < count; ++i) {
      f32 px = static_cast<f32>(x + i) + 0.5f;
      f32 py = static_cast<f32>(y) + 0.5f;

      if (!localMatrix_.isIdentity()) {
        bool ok;
        Matrix inv = localMatrix_.inverted(&ok);
        if (ok) {
          Point mapped = inv.mapPoint({px, py});
          px = mapped.x;
          py = mapped.y;
        }
      }

      f32 dx = px - center_.x, dy = py - center_.y;
      f32 dist = std::sqrt(dx * dx + dy * dy);
      f32 t = applyTileMode(dist / radius_, mode_);
      out[i] = grad_.evaluate(t);
    }
  }
};

class SweepGradientImpl : public Shader {
public:
  Point center_;
  GradientBase grad_;

  SweepGradientImpl(Point center, const Color *colors, const f32 *positions,
                    i32 count)
      : center_(center) {
    grad_.init(colors, positions, count);
  }

  void shadeSpan(i32 x, i32 y, i32 count, Color *out) const override {
    constexpr f32 kTwoPi = 6.28318530718f;
    for (i32 i = 0; i < count; ++i) {
      f32 px = static_cast<f32>(x + i) + 0.5f;
      f32 py = static_cast<f32>(y) + 0.5f;

      if (!localMatrix_.isIdentity()) {
        bool ok;
        Matrix inv = localMatrix_.inverted(&ok);
        if (ok) {
          Point mapped = inv.mapPoint({px, py});
          px = mapped.x;
          py = mapped.y;
        }
      }

      f32 angle = std::atan2(py - center_.y, px - center_.x);
      f32 t = (angle + 3.14159265f) / kTwoPi;
      t = clampF(t, 0.0f, 1.0f);
      out[i] = grad_.evaluate(t);
    }
  }
};

} // anonymous namespace

namespace Shaders {

std::shared_ptr<Shader> MakeLinear(Point start, Point end, const Color *colors,
                                   const f32 *positions, i32 count,
                                   TileMode mode) {
  if (!colors || count < 2)
    return nullptr;
  return std::make_shared<LinearGradientImpl>(start, end, colors, positions,
                                              count, mode);
}

std::shared_ptr<Shader> MakeRadial(Point center, f32 radius,
                                   const Color *colors, const f32 *positions,
                                   i32 count, TileMode mode) {
  if (!colors || count < 2 || radius <= 0)
    return nullptr;
  return std::make_shared<RadialGradientImpl>(center, radius, colors, positions,
                                              count, mode);
}

std::shared_ptr<Shader> MakeSweep(Point center, const Color *colors,
                                  const f32 *positions, i32 count) {
  if (!colors || count < 2)
    return nullptr;
  return std::make_shared<SweepGradientImpl>(center, colors, positions, count);
}

} // namespace Shaders

} // namespace ink
