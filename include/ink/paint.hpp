#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Shader;
class ColorFilter;
class PathEffect;

/// @brief Blend mode for compositing.
enum class BlendMode : u8 {
  SrcOver = 0,
  Src,
  Dst,
  SrcIn,
  DstIn,
  SrcOut,
  DstOut,
  SrcAtop,
  DstAtop,
  Xor,
  Clear,
};

/// @brief Paint style.
enum class PaintStyle : u8 {
  Fill,
  Stroke,
  FillAndStroke,
};

/// @brief Image sampling quality.
enum class FilterQuality : u8 {
  None,   ///< Nearest neighbor.
  Low,    ///< Bilinear.
  Medium, ///< Bilinear with mipmaps.
  High,   ///< Bicubic.
};

/// @brief Describes how to draw: color, style, blend mode, stroke width,
/// opacity.
struct Paint {
  Color color = {0, 0, 0, 255};
  f32 strokeWidth = 1.0f;
  f32 opacity = 1.0f;
  BlendMode blendMode = BlendMode::SrcOver;
  PaintStyle style = PaintStyle::Fill;

  /// Stroke parameters (used when style includes Stroke).
  u8 cap = 0;  ///< LineCap (0=Butt, 1=Round, 2=Square)
  u8 join = 0; ///< LineJoin (0=Miter, 1=Round, 2=Bevel)
  f32 miterLimit = 4.0f;
  bool antiAlias = true;
  FilterQuality filterQuality = FilterQuality::Low;

  /// Optional shader (gradient, pattern) — overrides color.
  std::shared_ptr<Shader> shader;

  /// Optional color filter applied after shading.
  std::shared_ptr<ColorFilter> colorFilter;

  /// Optional geometry effect applied before path drawing/stroking.
  std::shared_ptr<PathEffect> pathEffect;

  explicit Paint() = default;

  static Paint Fill(Color c) {
    Paint p;
    p.color = c;
    p.style = PaintStyle::Fill;
    return p;
  }

  static Paint Stroke(Color c, f32 width = 1.0f) {
    Paint p;
    p.color = c;
    p.strokeWidth = width;
    p.style = PaintStyle::Stroke;
    return p;
  }

  u8 effectiveAlpha() const { return u8(color.a * opacity); }
};

} // namespace ink
