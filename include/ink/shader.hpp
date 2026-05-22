#pragma once

/**
 * @file shader.hpp
 * @brief Shader base class, gradient shaders, and image pattern shader.
 */

#include "ink/matrix.hpp"
#include "ink/types.hpp"
#include <memory>
#include <vector>

namespace ink {

/// @brief Tile mode for shaders at their boundaries.
enum class TileMode : u8 {
  Clamp,  ///< Clamp to edge color.
  Repeat, ///< Repeat the pattern.
  Mirror, ///< Mirror the pattern.
};

/// @brief A gradient color stop.
struct GradientStop {
  f32 position; ///< Position in [0, 1].
  Color color;  ///< Color at this position.
};

/// @brief Abstract shader that generates colors for a span of pixels.
///
/// Shaders plug into Paint to provide per-pixel coloring (gradients,
/// image patterns, etc.) beyond simple flat color.
class Shader {
public:
  virtual ~Shader() = default;

  /// @brief Generate colors for a horizontal span of pixels.
  /// @param x Starting x coordinate.
  /// @param y Y coordinate.
  /// @param count Number of pixels.
  /// @param out Output color array (must hold count elements).
  virtual void shadeSpan(i32 x, i32 y, i32 count, Color *out) const = 0;

  void setLocalMatrix(const Matrix &m) { localMatrix_ = m; }
  const Matrix &localMatrix() const { return localMatrix_; }

protected:
  Matrix localMatrix_;
};

/// @brief Factory functions for creating shader instances.
namespace Shaders {

/// @brief Create a linear gradient shader.
/// @param start Start point of the gradient line.
/// @param end End point of the gradient line.
/// @param colors Array of colors.
/// @param positions Array of stop positions (may be nullptr for uniform).
/// @param count Number of color stops (must be >= 2).
/// @param mode Tile mode at gradient boundaries.
/// @return Shared pointer to the new shader.
std::shared_ptr<Shader> MakeLinear(Point start, Point end, const Color *colors,
                                   const f32 *positions, i32 count,
                                   TileMode mode = TileMode::Clamp);

/// @brief Create a radial gradient shader.
/// @param center Center of the gradient.
/// @param radius Radius of the gradient.
/// @param colors Array of colors.
/// @param positions Array of stop positions (may be nullptr for uniform).
/// @param count Number of color stops (must be >= 2).
/// @param mode Tile mode at gradient boundaries.
/// @return Shared pointer to the new shader.
std::shared_ptr<Shader> MakeRadial(Point center, f32 radius,
                                   const Color *colors, const f32 *positions,
                                   i32 count, TileMode mode = TileMode::Clamp);

/// @brief Create a sweep (conic/angular) gradient shader.
/// @param center Center of the gradient.
/// @param colors Array of colors.
/// @param positions Array of stop positions (may be nullptr for uniform).
/// @param count Number of color stops (must be >= 2).
/// @return Shared pointer to the new shader.
std::shared_ptr<Shader> MakeSweep(Point center, const Color *colors,
                                  const f32 *positions, i32 count);

} // namespace Shaders

} // namespace ink
