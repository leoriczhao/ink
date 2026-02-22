#pragma once

/**
 * @file glyph_cache.hpp
 * @brief Font rasterization cache with texture atlas for text rendering.
 */

#include "ink/types.hpp"
#include <string_view>
#include <unordered_map>
#include <vector>
#include <string>

namespace ink {

/// @brief Metrics for a single rasterized glyph.
struct GlyphMetrics {
    i32 x0, y0, x1, y1; ///< Bounding box in pixels.
    i32 advance;         ///< Horizontal advance width.
    f32 u0, v0, u1, v1; ///< Texture atlas UV coordinates.
};

/// @brief Font rasterization cache backed by a texture atlas.
///
/// Rasterizes glyphs on demand using stb_truetype and stores them in a
/// greyscale atlas that can be uploaded to the GPU.
class GlyphCache {
public:
    ~GlyphCache();

    /// @brief Initialize with a TrueType font file and pixel size.
    /// @param fontPath Path to the .ttf font file.
    /// @param fontSize Desired font size in pixels.
    /// @return True on success, false if the font could not be loaded.
    bool init(const char* fontPath, f32 fontSize);

    /// @brief Release all resources.
    void release();

    /// @brief Get (or rasterize) metrics for a character.
    /// @param ch The character to look up.
    /// @return Pointer to the glyph metrics, or nullptr on failure.
    const GlyphMetrics* getGlyph(char ch);

    /// @brief Get the raw atlas pixel data.
    /// @return Pointer to the greyscale atlas buffer.
    const u8* atlasData() const { return atlas_.data(); }

    /// @brief Get the atlas width.
    /// @return Width in pixels.
    i32 atlasWidth() const { return atlasW_; }

    /// @brief Get the atlas height.
    /// @return Height in pixels.
    i32 atlasHeight() const { return atlasH_; }

    /// @brief Check if the atlas has been modified since last upload.
    /// @return True if dirty.
    bool atlasDirty() const { return dirty_; }

    /// @brief Mark the atlas as clean (after uploading to GPU).
    void markClean() { dirty_ = false; }

    /// @brief Get the line height for the loaded font.
    /// @return Line height in pixels.
    i32 lineHeight() const { return lineHeight_; }

    /// @brief Get the font ascent.
    /// @return Ascent in pixels.
    i32 ascent() const { return ascent_; }

    /// @brief Rasterize text directly into a pixel buffer (CPU path).
    /// @param pixels Destination pixel buffer (RGBA u32).
    /// @param stride Row stride of the destination buffer in pixels.
    /// @param bufH Height of the destination buffer in pixels.
    /// @param x X position to start drawing.
    /// @param y Y position to start drawing.
    /// @param text UTF-8 text to rasterize.
    /// @param c Text color.
    void drawText(u32* pixels, i32 stride, i32 bufH,
                  i32 x, i32 y, std::string_view text, Color c);

    /// @brief Measure the horizontal advance width of a text string.
    /// @param text UTF-8 text to measure.
    /// @return Width in pixels.
    i32 measureText(std::string_view text);
    
private:
    std::vector<u8> fontData_;
    void* fontInfo_ = nullptr;
    f32 scale_ = 0;
    i32 ascent_ = 0, descent_ = 0, lineGap_ = 0, lineHeight_ = 0;
    
    std::vector<u8> atlas_;
    i32 atlasW_ = 512, atlasH_ = 256;
    i32 cursorX_ = 1, cursorY_ = 1, rowHeight_ = 0;
    bool dirty_ = true;
    
    std::unordered_map<char, GlyphMetrics> glyphs_;
    
    bool rasterizeGlyph(char ch);
    bool growAtlas();
};

}
