#pragma once

/**
 * @file font.hpp
 * @brief Font class — a Typeface at a specific size with rendering options.
 */

#include "ink/typeface.hpp"
#include "ink/types.hpp"
#include <memory>
#include <string_view>

namespace ink {

/// @brief A font instance: typeface + size + rendering options.
class Font {
public:
  Font() = default;

  /// @brief Create a font from a typeface and size.
  Font(std::shared_ptr<Typeface> typeface, f32 size);

  /// @brief Get the typeface.
  std::shared_ptr<Typeface> typeface() const { return typeface_; }

  /// @brief Get the font size.
  f32 size() const { return size_; }

  /// @brief Set the font size.
  void setSize(f32 s) { size_ = s; }

  /// @brief Check if the font is valid.
  bool valid() const { return typeface_ && typeface_->valid(); }

  /// @brief Measure text width.
  i32 measureText(std::string_view text) const;

  /// @brief Get line height.
  i32 lineHeight() const;

  /// @brief Get the underlying GlyphCache for this size.
  GlyphCache *glyphCache() const;

private:
  std::shared_ptr<Typeface> typeface_;
  f32 size_ = 16.0f;
};

} // namespace ink
