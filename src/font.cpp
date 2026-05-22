#include "ink/font.hpp"
#include "ink/color_filter.hpp"
#include "ink/text_blob.hpp"
#include "ink/typeface.hpp"
#include <algorithm>
#include <cmath>

namespace ink {

// --- Typeface ---

Typeface::Typeface(const char *path) : path_(path) {}

std::shared_ptr<Typeface> Typeface::MakeFromFile(const char *path) {
  auto tf = std::shared_ptr<Typeface>(new Typeface(path));
  auto cache = std::make_unique<GlyphCache>();
  if (cache->init(path, 16.0f)) {
    tf->valid_ = true;
    tf->caches_.push_back({16.0f, std::move(cache)});
  }
  return tf->valid_ ? tf : nullptr;
}

GlyphCache *Typeface::getCache(f32 fontSize) {
  for (auto &sc : caches_) {
    if (std::abs(sc.size - fontSize) < 0.01f)
      return sc.cache.get();
  }
  auto cache = std::make_unique<GlyphCache>();
  if (!cache->init(path_.c_str(), fontSize))
    return nullptr;
  caches_.push_back({fontSize, std::move(cache)});
  return caches_.back().cache.get();
}

// --- Font ---

Font::Font(std::shared_ptr<Typeface> typeface, f32 size)
    : typeface_(std::move(typeface)), size_(size) {}

i32 Font::measureText(std::string_view text) const {
  auto *gc = glyphCache();
  return gc ? gc->measureText(text) : 0;
}

i32 Font::lineHeight() const {
  auto *gc = glyphCache();
  return gc ? gc->lineHeight() : 0;
}

GlyphCache *Font::glyphCache() const {
  return typeface_ ? typeface_->getCache(size_) : nullptr;
}

// --- TextBlob ---

TextBlob::TextBlob(std::string text, Font font, i32 w, i32 h)
    : text_(std::move(text)), font_(std::move(font)), width_(w), height_(h) {}

std::shared_ptr<TextBlob> TextBlob::Make(std::string_view text,
                                         const Font &font) {
  if (!font.valid())
    return nullptr;
  i32 w = font.measureText(text);
  i32 h = font.lineHeight();
  return std::shared_ptr<TextBlob>(new TextBlob(std::string(text), font, w, h));
}

// --- ColorFilter implementations ---

namespace {

class MatrixColorFilterImpl : public ColorFilter {
public:
  f32 m_[20];

  explicit MatrixColorFilterImpl(const f32 matrix[20]) {
    std::copy(matrix, matrix + 20, m_);
  }

  Color filterColor(Color c) const override {
    f32 r = c.r, g = c.g, b = c.b, a = c.a;
    auto cl = [](f32 v) -> u8 {
      return static_cast<u8>(std::max(0.0f, std::min(255.0f, v + 0.5f)));
    };
    return {
        cl(m_[0] * r + m_[1] * g + m_[2] * b + m_[3] * a + m_[4] * 255),
        cl(m_[5] * r + m_[6] * g + m_[7] * b + m_[8] * a + m_[9] * 255),
        cl(m_[10] * r + m_[11] * g + m_[12] * b + m_[13] * a + m_[14] * 255),
        cl(m_[15] * r + m_[16] * g + m_[17] * b + m_[18] * a + m_[19] * 255),
    };
  }
};

class BlendModeColorFilterImpl : public ColorFilter {
public:
  Color blendColor_;
  BlendMode mode_;

  BlendModeColorFilterImpl(Color c, BlendMode mode)
      : blendColor_(c), mode_(mode) {}

  Color filterColor(Color c) const override {
    // Simplified blend: use blendColor as src, c as dst
    u32 sa = blendColor_.a, da = c.a;
    u32 invSa = 255 - sa;
    return {
        static_cast<u8>((blendColor_.r * sa + c.r * da * invSa / 255) / 255),
        static_cast<u8>((blendColor_.g * sa + c.g * da * invSa / 255) / 255),
        static_cast<u8>((blendColor_.b * sa + c.b * da * invSa / 255) / 255),
        static_cast<u8>((sa * 255 + da * invSa) / 255),
    };
  }
};

class ComposeColorFilterImpl : public ColorFilter {
public:
  std::shared_ptr<ColorFilter> outer_, inner_;

  ComposeColorFilterImpl(std::shared_ptr<ColorFilter> outer,
                         std::shared_ptr<ColorFilter> inner)
      : outer_(std::move(outer)), inner_(std::move(inner)) {}

  Color filterColor(Color c) const override {
    Color mid = inner_ ? inner_->filterColor(c) : c;
    return outer_ ? outer_->filterColor(mid) : mid;
  }
};

} // anonymous namespace

namespace ColorFilters {

std::shared_ptr<ColorFilter> MakeMatrix(const f32 matrix[20]) {
  if (!matrix)
    return nullptr;
  return std::make_shared<MatrixColorFilterImpl>(matrix);
}

std::shared_ptr<ColorFilter> MakeBlend(Color c, BlendMode mode) {
  return std::make_shared<BlendModeColorFilterImpl>(c, mode);
}

std::shared_ptr<ColorFilter> MakeCompose(std::shared_ptr<ColorFilter> outer,
                                         std::shared_ptr<ColorFilter> inner) {
  return std::make_shared<ComposeColorFilterImpl>(std::move(outer),
                                                  std::move(inner));
}

} // namespace ColorFilters

} // namespace ink
