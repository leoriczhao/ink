#pragma once

#include "ink/types.hpp"

namespace ink {

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

/// @brief Describes how to draw: color, style, blend mode, stroke width, opacity.
struct Paint {
    Color color = {0, 0, 0, 255};
    f32 strokeWidth = 1.0f;
    f32 opacity = 1.0f;
    BlendMode blendMode = BlendMode::SrcOver;
    PaintStyle style = PaintStyle::Fill;

    /// @brief Default construct a paint (black, fill, SrcOver, full opacity).
    explicit Paint() = default;

    /// @brief Construct a fill paint from a color.
    static Paint Fill(Color c) {
        Paint p;
        p.color = c;
        p.style = PaintStyle::Fill;
        return p;
    }

    /// @brief Construct a stroke paint.
    static Paint Stroke(Color c, f32 width = 1.0f) {
        Paint p;
        p.color = c;
        p.strokeWidth = width;
        p.style = PaintStyle::Stroke;
        return p;
    }

    /// @brief Compute effective alpha (color.a * opacity).
    u8 effectiveAlpha() const {
        return u8(color.a * opacity);
    }
};

} // namespace ink
