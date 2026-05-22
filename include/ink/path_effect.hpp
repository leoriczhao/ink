#pragma once

/**
 * @file path_effect.hpp
 * @brief Path effect abstraction and dash path effect factory.
 */

#include "ink/path.hpp"
#include <memory>

namespace ink {

/// @brief Abstract path effect that transforms geometry before drawing.
class PathEffect {
public:
  virtual ~PathEffect() = default;

  /// @brief Return a transformed copy of the source path.
  virtual Path filterPath(const Path &src) const = 0;
};

/// @brief Factory functions for creating path effects.
namespace PathEffects {

/// @brief Create a dash effect from alternating on/off intervals.
/// @param intervals Positive interval lengths. Odd counts are repeated to form
/// an even pattern.
/// @param count Number of interval entries.
/// @param phase Initial distance into the pattern.
/// @return Shared pointer to the effect, or nullptr for invalid intervals.
std::shared_ptr<PathEffect> MakeDash(const f32 *intervals, i32 count,
                                     f32 phase = 0.0f);

} // namespace PathEffects

} // namespace ink
