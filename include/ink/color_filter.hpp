#pragma once

/**
 * @file color_filter.hpp
 * @brief Color filter abstraction for per-pixel color transformation.
 */

#include "ink/paint.hpp"
#include "ink/types.hpp"
#include <array>
#include <memory>

namespace ink {

/// @brief Abstract color filter that transforms colors during rendering.
class ColorFilter {
public:
  virtual ~ColorFilter() = default;

  /// @brief Filter a single color.
  /// @param c Input color.
  /// @return Transformed color.
  virtual Color filterColor(Color c) const = 0;

  /// @brief Filter a span of colors in-place.
  /// @param colors Array of colors to transform.
  /// @param count Number of colors.
  virtual void filterSpan(Color *colors, i32 count) const {
    for (i32 i = 0; i < count; ++i)
      colors[i] = filterColor(colors[i]);
  }
};

/// @brief Factory functions for creating color filter instances.
namespace ColorFilters {

/// @brief Create a 5x4 color matrix filter.
///
/// The matrix is applied as:
///   R' = m[0]*R + m[1]*G + m[2]*B  + m[3]*A  + m[4]*255
///   G' = m[5]*R + m[6]*G + m[7]*B  + m[8]*A  + m[9]*255
///   B' = m[10]*R+ m[11]*G+ m[12]*B + m[13]*A + m[14]*255
///   A' = m[15]*R+ m[16]*G+ m[17]*B + m[18]*A + m[19]*255
///
/// @param matrix Array of 20 floats (row-major 5x4 matrix).
/// @return Shared pointer to the new filter.
std::shared_ptr<ColorFilter> MakeMatrix(const f32 matrix[20]);

/// @brief Create a filter that blends every color with a fixed color.
/// @param c The blend color.
/// @param mode The blend mode to apply.
/// @return Shared pointer to the new filter.
std::shared_ptr<ColorFilter> MakeBlend(Color c, BlendMode mode);

/// @brief Create a filter that chains two filters (outer applied after inner).
/// @param outer The second filter to apply.
/// @param inner The first filter to apply.
/// @return Shared pointer to the composed filter.
std::shared_ptr<ColorFilter> MakeCompose(std::shared_ptr<ColorFilter> outer,
                                         std::shared_ptr<ColorFilter> inner);

} // namespace ColorFilters

} // namespace ink
