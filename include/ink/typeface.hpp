#pragma once

/**
 * @file typeface.hpp
 * @brief Typeface abstraction — represents a specific font file.
 */

#include "ink/glyph_cache.hpp"
#include "ink/types.hpp"
#include <memory>
#include <string>

namespace ink {

/// @brief Represents a font file (e.g. a .ttf file).
///
/// Typeface wraps the raw font data and can produce sized Font instances.
class Typeface {
public:
  /// @brief Load a typeface from a TrueType font file.
  /// @param path Path to the .ttf file.
  /// @return Shared pointer, or nullptr on failure.
  static std::shared_ptr<Typeface> MakeFromFile(const char *path);

  /// @brief Get the file path this typeface was loaded from.
  const std::string &path() const { return path_; }

  /// @brief Check if the typeface is valid.
  bool valid() const { return valid_; }

  /// @brief Get or create a GlyphCache at the given font size.
  ///
  /// Caches are created lazily and reused for the same size.
  GlyphCache *getCache(f32 fontSize);

private:
  explicit Typeface(const char *path);

  std::string path_;
  bool valid_ = false;
  struct SizedCache {
    f32 size;
    std::unique_ptr<GlyphCache> cache;
  };
  std::vector<SizedCache> caches_;
};

} // namespace ink
