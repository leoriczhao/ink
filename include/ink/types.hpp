#pragma once

#include <cstdint>

/**
 * @file types.hpp
 * @brief Core type aliases and basic geometric/color types for the ink library.
 */

namespace ink {

using i32 = int32_t;   ///< Signed 32-bit integer.
using u32 = uint32_t;  ///< Unsigned 32-bit integer.
using u64 = uint64_t;  ///< Unsigned 64-bit integer.
using u16 = uint16_t;  ///< Unsigned 16-bit integer.
using u8 = uint8_t;    ///< Unsigned 8-bit integer.
using f32 = float;     ///< 32-bit floating point.
using f64 = double;    ///< 64-bit floating point.

/// @brief A 2D point with floating-point coordinates.
struct Point {
    f32 x = 0;  ///< X coordinate.
    f32 y = 0;  ///< Y coordinate.
};

/// @brief An axis-aligned rectangle defined by position and size.
struct Rect {
    f32 x = 0;  ///< Left edge X coordinate.
    f32 y = 0;  ///< Top edge Y coordinate.
    f32 w = 0;  ///< Width.
    f32 h = 0;  ///< Height.
};

/// @brief An RGBA color with 8-bit components.
struct Color {
    u8 r = 0;    ///< Red component (0–255).
    u8 g = 0;    ///< Green component (0–255).
    u8 b = 0;    ///< Blue component (0–255).
    u8 a = 255;  ///< Alpha component (0–255), default opaque.
};

}
