#pragma once

#include "ink/types.hpp"
#include <string_view>
#include <unordered_map>
#include <vector>
#include <string>

namespace ink {

struct GlyphMetrics {
    i32 x0, y0, x1, y1;
    i32 advance;
    f32 u0, v0, u1, v1;
};

class GlyphCache {
public:
    ~GlyphCache();
    
    bool init(const char* fontPath, f32 fontSize);
    void release();
    
    const GlyphMetrics* getGlyph(char ch);
    
    const u8* atlasData() const { return atlas_.data(); }
    i32 atlasWidth() const { return atlasW_; }
    i32 atlasHeight() const { return atlasH_; }
    bool atlasDirty() const { return dirty_; }
    void markClean() { dirty_ = false; }
    
    i32 lineHeight() const { return lineHeight_; }
    i32 ascent() const { return ascent_; }
    
    void drawText(u32* pixels, i32 stride, i32 bufH,
                  i32 x, i32 y, std::string_view text, Color c);
    
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
