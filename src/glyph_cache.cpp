#define STB_TRUETYPE_IMPLEMENTATION
#include "ink/glyph_cache.hpp"
#include "../third_party/stb_truetype.h"
#include <fstream>
#include <cstring>

namespace ink {

GlyphCache::~GlyphCache() {
    release();
}

bool GlyphCache::init(const char* fontPath, f32 fontSize) {
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file) return false;
    
    auto size = file.tellg();
    file.seekg(0);
    fontData_.resize(size);
    file.read(reinterpret_cast<char*>(fontData_.data()), size);
    
    fontInfo_ = new stbtt_fontinfo;
    auto* info = static_cast<stbtt_fontinfo*>(fontInfo_);
    if (!stbtt_InitFont(info, fontData_.data(), 0)) {
        delete info;
        fontInfo_ = nullptr;
        return false;
    }
    
    scale_ = stbtt_ScaleForPixelHeight(info, fontSize);
    stbtt_GetFontVMetrics(info, &ascent_, &descent_, &lineGap_);
    ascent_ = i32(ascent_ * scale_);
    descent_ = i32(descent_ * scale_);
    lineHeight_ = ascent_ - descent_ + i32(lineGap_ * scale_);
    
    atlas_.resize(atlasW_ * atlasH_, 0);
    dirty_ = true;
    
    return true;
}

void GlyphCache::release() {
    if (fontInfo_) {
        delete static_cast<stbtt_fontinfo*>(fontInfo_);
        fontInfo_ = nullptr;
    }
    fontData_.clear();
    atlas_.clear();
    glyphs_.clear();
}

const GlyphMetrics* GlyphCache::getGlyph(char ch) {
    auto it = glyphs_.find(ch);
    if (it != glyphs_.end()) return &it->second;
    
    if (!rasterizeGlyph(ch)) return nullptr;
    return &glyphs_[ch];
}

bool GlyphCache::rasterizeGlyph(char ch) {
    if (!fontInfo_) return false;
    auto* info = static_cast<stbtt_fontinfo*>(fontInfo_);
    
    i32 advance, lsb;
    stbtt_GetCodepointHMetrics(info, ch, &advance, &lsb);
    
    i32 x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(info, ch, scale_, scale_, &x0, &y0, &x1, &y1);
    
    i32 glyphW = x1 - x0;
    i32 glyphH = y1 - y0;
    
    if (glyphW <= 0 || glyphH <= 0) {
        GlyphMetrics m = {};
        m.advance = i32(advance * scale_);
        m.x0 = x0; m.y0 = y0; m.x1 = x1; m.y1 = y1;
        glyphs_[ch] = m;
        return true;
    }
    
    if (cursorX_ + glyphW + 1 > atlasW_) {
        cursorX_ = 1;
        cursorY_ += rowHeight_ + 1;
        rowHeight_ = 0;
    }
    
    if (cursorY_ + glyphH + 1 > atlasH_) {
        if (!growAtlas()) return false;
    }
    
    stbtt_MakeCodepointBitmap(info, 
        atlas_.data() + cursorY_ * atlasW_ + cursorX_,
        glyphW, glyphH, atlasW_, scale_, scale_, ch);
    
    GlyphMetrics m;
    m.x0 = x0; m.y0 = y0; m.x1 = x1; m.y1 = y1;
    m.advance = i32(advance * scale_);
    m.u0 = f32(cursorX_) / atlasW_;
    m.v0 = f32(cursorY_) / atlasH_;
    m.u1 = f32(cursorX_ + glyphW) / atlasW_;
    m.v1 = f32(cursorY_ + glyphH) / atlasH_;
    
    glyphs_[ch] = m;
    
    cursorX_ += glyphW + 1;
    if (glyphH > rowHeight_) rowHeight_ = glyphH;
    dirty_ = true;
    
    return true;
}

bool GlyphCache::growAtlas() {
    i32 newH = atlasH_ * 2;
    atlas_.resize(atlasW_ * newH, 0);
    atlasH_ = newH;
    
    for (auto& [ch, m] : glyphs_) {
        m.v0 = m.v0 * 0.5f;
        m.v1 = m.v1 * 0.5f;
    }
    
    dirty_ = true;
    return true;
}

void GlyphCache::drawText(u32* pixels, i32 stride, i32 bufH,
                          i32 x, i32 y, std::string_view text, Color c) {
    i32 penX = x;
    i32 baseline = y + ascent_;
    
    for (char ch : text) {
        const GlyphMetrics* g = getGlyph(ch);
        if (!g) continue;
        
        i32 glyphW = g->x1 - g->x0;
        i32 glyphH = g->y1 - g->y0;
        
        if (glyphW > 0 && glyphH > 0) {
            i32 dstX = penX + g->x0;
            i32 dstY = baseline + g->y0;
            
            i32 srcX = i32(g->u0 * atlasW_);
            i32 srcY = i32(g->v0 * atlasH_);
            
            for (i32 row = 0; row < glyphH; ++row) {
                i32 dy = dstY + row;
                if (dy < 0 || dy >= bufH) continue;
                
                for (i32 col = 0; col < glyphW; ++col) {
                    i32 dx = dstX + col;
                    if (dx < 0 || dx >= stride) continue;
                    
                    u8 alpha = atlas_[(srcY + row) * atlasW_ + srcX + col];
                    if (alpha == 0) continue;
                    
                    u32& dst = pixels[dy * stride + dx];
                    u8 dstR = (dst >> 16) & 0xFF;
                    u8 dstG = (dst >> 8) & 0xFF;
                    u8 dstB = dst & 0xFF;
                    
                    u8 outR = u8((c.r * alpha + dstR * (255 - alpha)) / 255);
                    u8 outG = u8((c.g * alpha + dstG * (255 - alpha)) / 255);
                    u8 outB = u8((c.b * alpha + dstB * (255 - alpha)) / 255);
                    
                    dst = 0xFF000000 | (outR << 16) | (outG << 8) | outB;
                }
            }
        }
        
        penX += g->advance;
    }
}

i32 GlyphCache::measureText(std::string_view text) {
    i32 width = 0;
    for (char ch : text) {
        const GlyphMetrics* g = getGlyph(ch);
        if (g) width += g->advance;
    }
    return width;
}

}
