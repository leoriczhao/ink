#pragma once

/**
 * @file text_blob.hpp
 * @brief Pre-shaped text blob for efficient repeated rendering.
 */

#include "ink/font.hpp"
#include "ink/types.hpp"
#include <memory>
#include <string>

namespace ink {

/// @brief An immutable pre-shaped text run for efficient repeated rendering.
///
/// TextBlobs capture the text, font, and positioning so that the shaping
/// work is done once and the blob can be drawn many times.
class TextBlob {
public:
  /// @brief Create a TextBlob from text and a font.
  /// @param text UTF-8 text.
  /// @param font The font to shape with.
  /// @return Shared pointer to the blob, or nullptr on failure.
  static std::shared_ptr<TextBlob> Make(std::string_view text,
                                        const Font &font);

  /// @brief Get the text content.
  const std::string &text() const { return text_; }

  /// @brief Get the font used.
  const Font &font() const { return font_; }

  /// @brief Get measured width.
  i32 width() const { return width_; }

  /// @brief Get line height.
  i32 height() const { return height_; }

private:
  TextBlob(std::string text, Font font, i32 w, i32 h);

  std::string text_;
  Font font_;
  i32 width_ = 0;
  i32 height_ = 0;
};

} // namespace ink
