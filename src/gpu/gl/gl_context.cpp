// GL context implementation - implements GpuImpl for OpenGL.
//
// Only compiled when INK_HAS_GL is defined (via CMake).
// Requires OpenGL 3.3+ core profile.

#include "ink/gpu/gl/gl_context.hpp"
#include "gl_resources.hpp"
#include "gpu_impl.hpp"
#include "ink/color_filter.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/draw_pass.hpp"
#include "ink/glyph_cache.hpp"
#include "ink/gpu/gpu_context.hpp"
#include "ink/image.hpp"
#include "ink/paint.hpp"
#include "ink/path.hpp"
#include "ink/pixmap.hpp"
#include "ink/recording.hpp"
#include "ink/shader.hpp"
#include "path_rasterizer.hpp"
#include "stroke.hpp"

#if INK_HAS_GL

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace ink {

// Shader sources
static const char *kColorVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uProjection;
out vec4 vColor;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)";

static const char *kColorFragSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

static const char *kTextureVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
uniform mat4 uProjection;
out vec2 vTexCoord;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char *kTextureFragSrc = R"(
#version 330 core
in vec2 vTexCoord;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(uTexture, vTexCoord);
}
)";

// GLTextureCache::resolve implementation
GLuint GLTextureCache::resolve(const Image *image) {
  if (!image || !image->valid())
    return 0;
  if (image->isGpuBacked())
    return static_cast<GLuint>(image->backendTextureHandle());

  u64 id = image->uniqueId();
  auto it = cache_.find(id);
  if (it != cache_.end())
    return it->second;

  GLuint tex = 0;
  glGenTextures(1, &tex);
  if (!tex)
    return 0;

  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width(), image->height(), 0,
               image->format() == PixelFormat::BGRA8888 ? GL_BGRA : GL_RGBA,
               GL_UNSIGNED_BYTE, image->pixels());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  cache_[id] = tex;
  return tex;
}

namespace {

constexpr u8 kClipStencilBit = 0x80;
constexpr u8 kPathStencilMask = 0x7F;
constexpr f32 kGLPathFlatTolerance = 0.25f;

struct GLPathContour {
  std::vector<Point> points;
};

f32 distToLineSq(Point p, Point a, Point b) {
  f32 dx = b.x - a.x;
  f32 dy = b.y - a.y;
  f32 lenSq = dx * dx + dy * dy;
  if (lenSq < 1e-12f) {
    f32 px = p.x - a.x;
    f32 py = p.y - a.y;
    return px * px + py * py;
  }
  f32 cross = (p.x - a.x) * dy - (p.y - a.y) * dx;
  return (cross * cross) / lenSq;
}

void flattenQuadInto(std::vector<Point> &out, Point p0, Point p1, Point p2,
                     f32 tol) {
  if (distToLineSq(p1, p0, p2) <= tol * tol) {
    out.push_back(p2);
    return;
  }
  Point q0 = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
  Point q1 = {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
  Point mid = {(q0.x + q1.x) * 0.5f, (q0.y + q1.y) * 0.5f};
  flattenQuadInto(out, p0, q0, mid, tol);
  flattenQuadInto(out, mid, q1, p2, tol);
}

void flattenCubicInto(std::vector<Point> &out, Point p0, Point p1, Point p2,
                      Point p3, f32 tol) {
  f32 d1 = distToLineSq(p1, p0, p3);
  f32 d2 = distToLineSq(p2, p0, p3);
  if (std::max(d1, d2) <= tol * tol) {
    out.push_back(p3);
    return;
  }
  Point ab = {(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
  Point bc = {(p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f};
  Point cd = {(p2.x + p3.x) * 0.5f, (p2.y + p3.y) * 0.5f};
  Point abc = {(ab.x + bc.x) * 0.5f, (ab.y + bc.y) * 0.5f};
  Point bcd = {(bc.x + cd.x) * 0.5f, (bc.y + cd.y) * 0.5f};
  Point mid = {(abc.x + bcd.x) * 0.5f, (abc.y + bcd.y) * 0.5f};
  flattenCubicInto(out, p0, ab, abc, mid, tol);
  flattenCubicInto(out, mid, bcd, cd, p3, tol);
}

Point evalConic(Point p0, Point p1, Point p2, f32 w, f32 t) {
  f32 mt = 1.0f - t;
  f32 a = mt * mt;
  f32 b = 2.0f * w * mt * t;
  f32 c = t * t;
  f32 denom = a + b + c;
  if (std::abs(denom) < 1e-8f)
    return p2;
  return {
      (a * p0.x + b * p1.x + c * p2.x) / denom,
      (a * p0.y + b * p1.y + c * p2.y) / denom,
  };
}

void flattenConicInto(std::vector<Point> &out, Point p0, Point p1, Point p2,
                      f32 w, f32 tol) {
  i32 steps =
      std::max(4, static_cast<i32>(std::ceil(
                      std::sqrt(std::max(distToLineSq(p1, p0, p2), 1.0f)) /
                      std::max(tol, 0.01f))));
  steps = std::min(steps, 64);
  for (i32 i = 1; i <= steps; ++i) {
    out.push_back(evalConic(p0, p1, p2, w, static_cast<f32>(i) / steps));
  }
}

std::vector<GLPathContour> flattenPathForGL(const Path &path,
                                            const Matrix &transform) {
  std::vector<GLPathContour> contours;
  const auto &verbs = path.verbs();
  const auto &pts = path.points();
  const auto &weights = path.conicWeights();
  i32 ptIdx = 0;
  i32 weightIdx = 0;
  Point cur = {0, 0};

  for (auto verb : verbs) {
    switch (verb) {
    case Path::Verb::Move:
      contours.push_back({});
      cur = transform.mapPoint(pts[ptIdx++]);
      contours.back().points.push_back(cur);
      break;
    case Path::Verb::Line: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      cur = transform.mapPoint(pts[ptIdx++]);
      contours.back().points.push_back(cur);
      break;
    }
    case Path::Verb::Quad: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      Point cp = transform.mapPoint(pts[ptIdx++]);
      Point end = transform.mapPoint(pts[ptIdx++]);
      flattenQuadInto(contours.back().points, cur, cp, end,
                      kGLPathFlatTolerance);
      cur = end;
      break;
    }
    case Path::Verb::Cubic: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      Point c1 = transform.mapPoint(pts[ptIdx++]);
      Point c2 = transform.mapPoint(pts[ptIdx++]);
      Point end = transform.mapPoint(pts[ptIdx++]);
      flattenCubicInto(contours.back().points, cur, c1, c2, end,
                       kGLPathFlatTolerance);
      cur = end;
      break;
    }
    case Path::Verb::Conic: {
      if (contours.empty()) {
        contours.push_back({});
        contours.back().points.push_back(cur);
      }
      Point cp = transform.mapPoint(pts[ptIdx++]);
      Point end = transform.mapPoint(pts[ptIdx++]);
      f32 w = weights[weightIdx++];
      flattenConicInto(contours.back().points, cur, cp, end, w,
                       kGLPathFlatTolerance);
      cur = end;
      break;
    }
    case Path::Verb::Close:
      break;
    }
  }

  for (auto &contour : contours) {
    auto &points = contour.points;
    if (points.size() > 1) {
      const Point &first = points.front();
      const Point &last = points.back();
      if (std::abs(first.x - last.x) < 1e-5f &&
          std::abs(first.y - last.y) < 1e-5f) {
        points.pop_back();
      }
    }
  }
  contours.erase(std::remove_if(contours.begin(), contours.end(),
                                [](const GLPathContour &c) {
                                  return c.points.size() < 3;
                                }),
                 contours.end());
  return contours;
}

Rect boundsForContours(const std::vector<GLPathContour> &contours) {
  if (contours.empty())
    return {};
  bool first = true;
  f32 minX = 0, minY = 0, maxX = 0, maxY = 0;
  for (const auto &contour : contours) {
    for (const Point &p : contour.points) {
      if (first) {
        minX = maxX = p.x;
        minY = maxY = p.y;
        first = false;
      } else {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
      }
    }
  }
  return {minX, minY, maxX - minX, maxY - minY};
}

} // namespace

// GLImpl - composes resource components
class GLImpl : public GpuImpl, public GLInterop, public DrawOpVisitor {
public:
  GLFramebuffer framebuffer_;
  GLShaderProgram colorProgram_;
  GLShaderProgram texProgram_;
  GLVertexBuffer<GLColorVertex> colorBuffer_;
  GLVertexBuffer<GLTexVertex> texBuffer_;
  GLTextureCache textureCache_;
  GLuint tempTexture_ = 0;
  GlyphCache *glyphCache_ = nullptr;
  BlendMode currentBlendMode_ = BlendMode::SrcOver;
  Matrix currentTransform_;
  std::vector<GLColorVertex> colorVerts_;
  std::vector<GLTexVertex> texVerts_;
  const Recording *currentRecording_ = nullptr;
  bool clipStencilActive_ = false;

  ~GLImpl() override { destroy(); }

  bool init(i32 w, i32 h) {
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    while (glGetError() != GL_NO_ERROR) {
    }
    if (err != GLEW_OK && glGetString(GL_VERSION) == nullptr) {
      std::fprintf(stderr, "ink GL: GLEW init failed\n");
      return false;
    }
    if (glGetString(GL_VERSION) == nullptr) {
      std::fprintf(stderr, "ink GL: no GL context\n");
      return false;
    }

    if (!colorProgram_.init(kColorVertSrc, kColorFragSrc))
      return false;
    colorProgram_.projLoc =
        glGetUniformLocation(colorProgram_.program, "uProjection");

    if (!texProgram_.init(kTextureVertSrc, kTextureFragSrc))
      return false;
    texProgram_.projLoc =
        glGetUniformLocation(texProgram_.program, "uProjection");
    texProgram_.samplerLoc =
        glGetUniformLocation(texProgram_.program, "uTexture");

    // Color VAO with position (2 float) + color (4 float)
    colorBuffer_.init();
    glBindVertexArray(colorBuffer_.vao);
    glBindBuffer(GL_ARRAY_BUFFER, colorBuffer_.vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLColorVertex),
                          (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLColorVertex),
                          (void *)8);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Texture VAO with position (2 float) + uv (2 float)
    texBuffer_.init();
    glBindVertexArray(texBuffer_.vao);
    glBindBuffer(GL_ARRAY_BUFFER, texBuffer_.vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLTexVertex),
                          (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLTexVertex),
                          (void *)8);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glGenTextures(1, &tempTexture_);

    return framebuffer_.init(w, h);
  }

  void destroy() {
    textureCache_.clear();
    framebuffer_.destroy();
    colorProgram_.destroy();
    texProgram_.destroy();
    colorBuffer_.destroy();
    texBuffer_.destroy();
    if (tempTexture_) {
      glDeleteTextures(1, &tempTexture_);
      tempTexture_ = 0;
    }
  }

  // GpuImpl interface
  bool valid() const override { return framebuffer_.fbo != 0; }

  void beginFrame(Color clearColor = {0, 0, 0, 255}) override {
    framebuffer_.bind();
    glClearColor(clearColor.r / 255.0f, clearColor.g / 255.0f,
                 clearColor.b / 255.0f, clearColor.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    currentBlendMode_ = BlendMode::SrcOver;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    clipStencilActive_ = false;
    glClear(GL_STENCIL_BUFFER_BIT);
  }

  void endFrame() override {
    flushColorBatch();
    glFlush();
  }

  void resize(i32 w, i32 h) override { framebuffer_.resize(w, h); }

  void setGlyphCache(GlyphCache *cache) override { glyphCache_ = cache; }

  void execute(const Recording &recording, const DrawPass &pass) override {
    currentRecording_ = &recording;
    recording.dispatch(*this, pass);
    flushColorBatch();
    currentRecording_ = nullptr;
  }

  void applyBlendMode(BlendMode mode) {
    if (mode == currentBlendMode_)
      return;
    flushColorBatch();
    currentBlendMode_ = mode;
    switch (mode) {
    case BlendMode::SrcOver:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case BlendMode::Src:
      glBlendFunc(GL_ONE, GL_ZERO);
      break;
    case BlendMode::Dst:
      glBlendFunc(GL_ZERO, GL_ONE);
      break;
    case BlendMode::SrcIn:
      glBlendFunc(GL_DST_ALPHA, GL_ZERO);
      break;
    case BlendMode::DstIn:
      glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
      break;
    case BlendMode::SrcOut:
      glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ZERO);
      break;
    case BlendMode::DstOut:
      glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case BlendMode::SrcAtop:
      glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case BlendMode::DstAtop:
      glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA);
      break;
    case BlendMode::Xor:
      glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case BlendMode::Clear:
      glBlendFunc(GL_ZERO, GL_ZERO);
      break;
    }
  }

  void applyOpacity(Color &c, u8 opacity) { c.a = u8(c.a * opacity / 255); }

  // DrawOpVisitor interface
  void visitFillRect(Rect r, Color c, BlendMode blend, u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    pushQuad(r.x, r.y, r.x + r.w, r.y + r.h, c);
  }

  void visitStrokeRect(Rect r, Color c, f32 width, BlendMode blend,
                       u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    float w = width > 0 ? width : 1.0f;
    pushQuad(r.x, r.y, r.x + r.w, r.y + w, c);
    pushQuad(r.x, r.y + r.h - w, r.x + r.w, r.y + r.h, c);
    pushQuad(r.x, r.y + w, r.x + w, r.y + r.h - w, c);
    pushQuad(r.x + r.w - w, r.y + w, r.x + r.w, r.y + r.h - w, c);
  }

  void visitLine(Point p1, Point p2, Color c, f32 width, BlendMode blend,
                 u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    pushLine(p1.x, p1.y, p2.x, p2.y, c, width > 0 ? width : 1.0f);
  }

  void visitPolyline(const Point *pts, i32 count, Color c, f32 width,
                     BlendMode blend, u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    float w = width > 0 ? width : 1.0f;
    for (i32 i = 0; i + 1 < count; ++i)
      pushLine(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, c, w);
  }

  void visitText(Point p, const char *text, u32 len, Color c, BlendMode blend,
                 u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    flushColorBatch();
    if (glyphCache_) {
      i32 tw = glyphCache_->measureText(std::string_view(text, len));
      i32 th = glyphCache_->lineHeight();
      if (tw > 0 && th > 0) {
        std::vector<u32> buf(tw * th, 0);
        glyphCache_->drawText(buf.data(), tw, th, 0, 0,
                              std::string_view(text, len), c);
        glBindTexture(GL_TEXTURE_2D, tempTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tw, th, 0, GL_BGRA,
                     GL_UNSIGNED_BYTE, buf.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        float x = p.x, y = p.y - th;
        pushTexQuad(x, y, x + tw, y + th, 0, 0, 1, 1);
        flushTexBatch(tempTexture_);
      }
    }
  }

  void visitDrawImage(const Image *image, f32 x, f32 y, BlendMode blend,
                      u8 opacity) override {
    (void)opacity;
    applyBlendMode(blend);
    flushColorBatch();
    if (image && image->valid()) {
      GLuint tex = textureCache_.resolve(image);
      if (tex) {
        float w = float(image->width()), h = float(image->height());
        pushTexQuad(x, y, x + w, y + h, 0, 0, 1, 1);
        flushTexBatch(tex);
      }
    }
  }

  void visitSetClip(Rect r) override {
    flushColorBatch();
    glEnable(GL_SCISSOR_TEST);
    glScissor(GLint(r.x), framebuffer_.height - GLint(r.y + r.h), GLsizei(r.w),
              GLsizei(r.h));
  }

  void visitSetClipPath(const Path &path, u8 fillRule,
                        bool antiAlias) override {
    (void)antiAlias;
    flushColorBatch();
    renderClipPathStencil(path,
                          fillRule ? FillRule::EvenOdd : FillRule::Winding);
  }

  void visitClearClip() override {
    flushColorBatch();
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    clipStencilActive_ = false;
  }

  void visitSetTransform(const Matrix &m) override {
    flushColorBatch();
    currentTransform_ = m;
  }

  void visitClearTransform() override {
    flushColorBatch();
    currentTransform_ = Matrix::Identity();
  }

  void visitFillPath(const Path &path, Color color, BlendMode blend, u8 opacity,
                     u8 fillRule, bool antiAlias, const Shader *shader,
                     const ColorFilter *) override {
    FillRule fr = fillRule ? FillRule::EvenOdd : FillRule::Winding;
    if (shader) {
      renderPathTexture(path, color, blend, opacity, antiAlias, fr, shader);
      return;
    }
    renderPathStencilCover(path, color, blend, opacity, fr);
  }

  void visitStrokePath(const Path &path, Color color, f32 width,
                       BlendMode blend, u8 opacity, u8 cap, u8 join,
                       f32 miterLimit, bool antiAlias, const Shader *shader,
                       const ColorFilter *) override {
    Path stroked =
        StrokeEngine::strokePath(path, width, static_cast<LineCap>(cap),
                                 static_cast<LineJoin>(join), miterLimit);
    if (shader) {
      renderPathTexture(stroked, color, blend, opacity, antiAlias,
                        FillRule::Winding, shader);
      return;
    }
    renderPathStencilCover(stroked, color, blend, opacity, FillRule::Winding);
  }

  void visitDrawImageRect(const Image *image, Rect src, Rect dst,
                          BlendMode blend, u8, FilterQuality) override {
    applyBlendMode(blend);
    flushColorBatch();
    if (image && image->valid()) {
      GLuint tex = textureCache_.resolve(image);
      if (tex) {
        float iw = float(image->width()), ih = float(image->height());
        float u0 = src.x / iw, v0 = src.y / ih;
        float u1 = (src.x + src.w) / iw, v1 = (src.y + src.h) / ih;
        pushTexQuad(dst.x, dst.y, dst.x + dst.w, dst.y + dst.h, u0, v0, u1, v1);
        flushTexBatch(tex);
      }
    }
  }

  void visitFillCircle(f32 cx, f32 cy, f32 radius, Color c, BlendMode blend,
                       u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    pushCircleFan(cx, cy, radius, c, 48);
  }

  void visitStrokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width,
                         BlendMode blend, u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    pushCircleRing(cx, cy, radius, c, width > 0 ? width : 1.0f, 48);
  }

  void visitFillRoundRect(Rect r, f32 rx, f32 ry, Color c, BlendMode blend,
                          u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    pushRoundRectFill(r, rx, ry, c, 16);
  }

  void visitStrokeRoundRect(Rect r, f32 rx, f32 ry, Color c, f32 width,
                            BlendMode blend, u8 opacity) override {
    applyBlendMode(blend);
    applyOpacity(c, opacity);
    pushRoundRectStroke(r, rx, ry, c, width > 0 ? width : 1.0f, 16);
  }

  std::shared_ptr<Image> makeSnapshot() const override {
    if (framebuffer_.width <= 0 || framebuffer_.height <= 0)
      return nullptr;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex)
      return nullptr;

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebuffer_.width,
                 framebuffer_.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    GLuint dstFbo = 0;
    glGenFramebuffers(1, &dstFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_.fbo);
    glBlitFramebuffer(0, 0, framebuffer_.width, framebuffer_.height, 0, 0,
                      framebuffer_.width, framebuffer_.height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glDeleteFramebuffers(1, &dstFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.fbo);

    auto owner = std::shared_ptr<GLuint>(new GLuint(tex), [](GLuint *t) {
      if (t && *t)
        glDeleteTextures(1, t);
      delete t;
    });
    return Image::MakeFromBackendTexture(tex, framebuffer_.width,
                                         framebuffer_.height,
                                         PixelFormat::RGBA8888, owner);
  }

  void readPixels(void *dst, i32 x, i32 y, i32 w, i32 h) const override {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.fbo);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dst);
  }

  unsigned int glTextureId() const override { return framebuffer_.texture; }
  unsigned int glFboId() const override { return framebuffer_.fbo; }

  GLInterop *glInterop() override { return this; }

  u64 resolveImageTexture(const Image *image) override {
    return textureCache_.resolve(image);
  }

  Point mapPoint(Point p) const {
    return currentTransform_.isIdentity() ? p : currentTransform_.mapPoint(p);
  }

  void restoreClipStencilState() {
    glStencilMask(0x00);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    if (clipStencilActive_) {
      glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_EQUAL, kClipStencilBit, kClipStencilBit);
    } else {
      glDisable(GL_STENCIL_TEST);
    }
  }

  void clearStencilBits(GLuint mask) {
    GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    if (scissorEnabled)
      glDisable(GL_SCISSOR_TEST);
    glStencilMask(mask);
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    if (scissorEnabled)
      glEnable(GL_SCISSOR_TEST);
  }

  void drawSolidRectNoTransform(Rect r, Color c) {
    if (r.w <= 0 || r.h <= 0)
      return;
    float x0 = r.x;
    float y0 = r.y;
    float x1 = r.x + r.w;
    float y1 = r.y + r.h;
    float cr = c.r / 255.0f, cg = c.g / 255.0f, cb = c.b / 255.0f,
          ca = c.a / 255.0f;
    std::vector<GLColorVertex> verts = {
        {x0, y0, cr, cg, cb, ca}, {x1, y0, cr, cg, cb, ca},
        {x0, y1, cr, cg, cb, ca}, {x1, y0, cr, cg, cb, ca},
        {x1, y1, cr, cg, cb, ca}, {x0, y1, cr, cg, cb, ca},
    };
    colorProgram_.use();
    colorProgram_.setProjection(float(framebuffer_.width),
                                float(framebuffer_.height));
    colorBuffer_.upload(verts);
    colorBuffer_.boundCount = GLsizei(verts.size());
    colorBuffer_.bind();
    glDrawArrays(GL_TRIANGLES, 0, colorBuffer_.boundCount);
    glBindVertexArray(0);
  }

  void drawContourFans(const std::vector<GLPathContour> &contours) {
    std::vector<GLColorVertex> verts;
    for (const auto &contour : contours) {
      const auto &pts = contour.points;
      if (pts.size() < 3)
        continue;
      for (size_t i = 1; i + 1 < pts.size(); ++i) {
        verts.push_back({pts[0].x, pts[0].y, 0, 0, 0, 0});
        verts.push_back({pts[i].x, pts[i].y, 0, 0, 0, 0});
        verts.push_back({pts[i + 1].x, pts[i + 1].y, 0, 0, 0, 0});
      }
    }
    if (verts.empty())
      return;
    colorProgram_.use();
    colorProgram_.setProjection(float(framebuffer_.width),
                                float(framebuffer_.height));
    colorBuffer_.upload(verts);
    colorBuffer_.boundCount = GLsizei(verts.size());
    colorBuffer_.bind();
    glDrawArrays(GL_TRIANGLES, 0, colorBuffer_.boundCount);
    glBindVertexArray(0);
  }

  void stencilPathBits(const std::vector<GLPathContour> &contours,
                       FillRule fillRule, bool restrictToCurrentClip) {
    glEnable(GL_STENCIL_TEST);
    glStencilMask(kPathStencilMask);
    if (restrictToCurrentClip) {
      glStencilFunc(GL_EQUAL, kClipStencilBit, kClipStencilBit);
    } else {
      glStencilFunc(GL_ALWAYS, 0, 0);
    }

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_CULL_FACE);
    if (fillRule == FillRule::EvenOdd) {
      glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
      drawContourFans(contours);
    } else {
      glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
      glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
      drawContourFans(contours);
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }

  void renderClipPathStencil(const Path &path, FillRule fillRule) {
    auto contours = flattenPathForGL(path, currentTransform_);
    bool hadClip = clipStencilActive_;

    clearStencilBits(kPathStencilMask);
    if (!contours.empty()) {
      stencilPathBits(contours, fillRule, hadClip);
    }

    clearStencilBits(kClipStencilBit);
    if (!contours.empty()) {
      Rect bounds = boundsForContours(contours);
      bounds.x -= 1.0f;
      bounds.y -= 1.0f;
      bounds.w += 2.0f;
      bounds.h += 2.0f;

      glEnable(GL_STENCIL_TEST);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glStencilMask(kClipStencilBit);
      glStencilFunc(GL_NOTEQUAL, kClipStencilBit, kPathStencilMask);
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      drawSolidRectNoTransform(bounds, {255, 255, 255, 255});
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    clearStencilBits(kPathStencilMask);
    clipStencilActive_ = true;
    restoreClipStencilState();
  }

  void renderPathStencilCover(const Path &path, Color color, BlendMode blend,
                              u8 opacity, FillRule fillRule) {
    if (framebuffer_.width <= 0 || framebuffer_.height <= 0)
      return;
    auto contours = flattenPathForGL(path, currentTransform_);
    if (contours.empty())
      return;

    flushColorBatch();
    applyBlendMode(blend);
    applyOpacity(color, opacity);

    clearStencilBits(kPathStencilMask);
    stencilPathBits(contours, fillRule, clipStencilActive_);

    Rect bounds = boundsForContours(contours);
    bounds.x -= 1.0f;
    bounds.y -= 1.0f;
    bounds.w += 2.0f;
    bounds.h += 2.0f;

    glStencilMask(0x00);
    glStencilFunc(GL_NOTEQUAL, 0, kPathStencilMask);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    drawSolidRectNoTransform(bounds, color);

    clearStencilBits(kPathStencilMask);
    restoreClipStencilState();
  }

  void renderPathTexture(const Path &path, Color color, BlendMode blend,
                         u8 opacity, bool antiAlias, FillRule fillRule,
                         const Shader *shader) {
    if (framebuffer_.width <= 0 || framebuffer_.height <= 0)
      return;
    flushColorBatch();
    applyBlendMode(blend);

    Pixmap pm = Pixmap::Alloc(
        PixmapInfo::MakeRGBA(framebuffer_.width, framebuffer_.height));
    if (!pm.valid())
      return;
    pm.clear({0, 0, 0, 0});

    PathRasterizer rasterizer;
    rasterizer.addPath(path, currentTransform_);
    rasterizer.rasterize(
        &pm, {0, 0, f32(framebuffer_.width), f32(framebuffer_.height)}, color,
        BlendMode::Src, opacity, antiAlias, fillRule, shader, false);

    glBindTexture(GL_TEXTURE_2D, tempTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebuffer_.width,
                 framebuffer_.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pm.addr());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    Matrix savedTransform = currentTransform_;
    currentTransform_ = Matrix::Identity();
    pushTexQuad(0, 0, float(framebuffer_.width), float(framebuffer_.height), 0,
                0, 1, 1);
    currentTransform_ = savedTransform;
    flushTexBatch(tempTexture_);
  }

  // Batching
  void pushQuad(float x0, float y0, float x1, float y1, Color c) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f,
          a = c.a / 255.0f;
    Point p00 = mapPoint({x0, y0});
    Point p10 = mapPoint({x1, y0});
    Point p01 = mapPoint({x0, y1});
    Point p11 = mapPoint({x1, y1});
    colorVerts_.insert(colorVerts_.end(), {{p00.x, p00.y, r, g, b, a},
                                           {p10.x, p10.y, r, g, b, a},
                                           {p01.x, p01.y, r, g, b, a},
                                           {p10.x, p10.y, r, g, b, a},
                                           {p11.x, p11.y, r, g, b, a},
                                           {p01.x, p01.y, r, g, b, a}});
  }

  void pushLine(float x0, float y0, float x1, float y1, Color c, float width) {
    Point p0 = mapPoint({x0, y0});
    Point p1 = mapPoint({x1, y1});
    x0 = p0.x;
    y0 = p0.y;
    x1 = p1.x;
    y1 = p1.y;
    float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.0001f)
      return;
    float hw = width * 0.5f, nx = -dy / len * hw, ny = dx / len * hw;
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f,
          a = c.a / 255.0f;
    colorVerts_.insert(colorVerts_.end(), {{x0 + nx, y0 + ny, r, g, b, a},
                                           {x0 - nx, y0 - ny, r, g, b, a},
                                           {x1 + nx, y1 + ny, r, g, b, a},
                                           {x0 - nx, y0 - ny, r, g, b, a},
                                           {x1 - nx, y1 - ny, r, g, b, a},
                                           {x1 + nx, y1 + ny, r, g, b, a}});
  }

  void pushTexQuad(float x0, float y0, float x1, float y1, float u0, float v0,
                   float u1, float v1) {
    Point p00 = mapPoint({x0, y0});
    Point p10 = mapPoint({x1, y0});
    Point p01 = mapPoint({x0, y1});
    Point p11 = mapPoint({x1, y1});
    texVerts_.insert(texVerts_.end(), {{p00.x, p00.y, u0, v0},
                                       {p10.x, p10.y, u1, v0},
                                       {p01.x, p01.y, u0, v1},
                                       {p10.x, p10.y, u1, v0},
                                       {p11.x, p11.y, u1, v1},
                                       {p01.x, p01.y, u0, v1}});
  }

  void pushCircleFan(float cx, float cy, float radius, Color c, int segments) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f,
          a = c.a / 255.0f;
    float step = 2.0f * 3.14159265f / segments;
    for (int i = 0; i < segments; ++i) {
      float a0 = i * step, a1 = (i + 1) * step;
      float x0 = cx + radius * std::cos(a0), y0 = cy + radius * std::sin(a0);
      float x1 = cx + radius * std::cos(a1), y1 = cy + radius * std::sin(a1);
      colorVerts_.insert(
          colorVerts_.end(),
          {{cx, cy, r, g, b, a}, {x0, y0, r, g, b, a}, {x1, y1, r, g, b, a}});
    }
  }

  void pushCircleRing(float cx, float cy, float radius, Color c, float width,
                      int segments) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f,
          a = c.a / 255.0f;
    float hw = width * 0.5f;
    float outerR = radius + hw, innerR = radius - hw;
    if (innerR < 0)
      innerR = 0;
    float step = 2.0f * 3.14159265f / segments;
    for (int i = 0; i < segments; ++i) {
      float a0 = i * step, a1 = (i + 1) * step;
      float cos0 = std::cos(a0), sin0 = std::sin(a0);
      float cos1 = std::cos(a1), sin1 = std::sin(a1);
      float ox0 = cx + outerR * cos0, oy0 = cy + outerR * sin0;
      float ox1 = cx + outerR * cos1, oy1 = cy + outerR * sin1;
      float ix0 = cx + innerR * cos0, iy0 = cy + innerR * sin0;
      float ix1 = cx + innerR * cos1, iy1 = cy + innerR * sin1;
      colorVerts_.insert(colorVerts_.end(), {{ox0, oy0, r, g, b, a},
                                             {ix0, iy0, r, g, b, a},
                                             {ox1, oy1, r, g, b, a},
                                             {ix0, iy0, r, g, b, a},
                                             {ix1, iy1, r, g, b, a},
                                             {ox1, oy1, r, g, b, a}});
    }
  }

  void pushRoundRectFill(Rect rect, float rx, float ry, Color c,
                         int cornerSegs) {
    rx = std::min(rx, rect.w * 0.5f);
    ry = std::min(ry, rect.h * 0.5f);
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f,
          a = c.a / 255.0f;
    float x = rect.x, y = rect.y, w = rect.w, h = rect.h;

    // Center cross (two rectangles)
    pushQuad(x + rx, y, x + w - rx, y + h, c);          // vertical strip
    pushQuad(x, y + ry, x + rx, y + h - ry, c);         // left strip
    pushQuad(x + w - rx, y + ry, x + w, y + h - ry, c); // right strip

    // Four corner fans
    float step = (3.14159265f * 0.5f) / cornerSegs;
    // Corners: TL, TR, BR, BL with center offsets and angle starts
    struct Corner {
      float cx, cy;
      float startAngle;
    };
    Corner corners[4] = {
        {x + rx, y + ry, 3.14159265f},            // top-left
        {x + w - rx, y + ry, 3.14159265f * 1.5f}, // top-right
        {x + w - rx, y + h - ry, 0.0f},           // bottom-right
        {x + rx, y + h - ry, 3.14159265f * 0.5f}  // bottom-left
    };
    for (auto &co : corners) {
      for (int i = 0; i < cornerSegs; ++i) {
        float a0 = co.startAngle + i * step;
        float a1 = co.startAngle + (i + 1) * step;
        float px0 = co.cx + rx * std::cos(a0), py0 = co.cy + ry * std::sin(a0);
        float px1 = co.cx + rx * std::cos(a1), py1 = co.cy + ry * std::sin(a1);
        colorVerts_.insert(colorVerts_.end(), {{co.cx, co.cy, r, g, b, a},
                                               {px0, py0, r, g, b, a},
                                               {px1, py1, r, g, b, a}});
      }
    }
  }

  void pushRoundRectStroke(Rect rect, float rx, float ry, Color c, float width,
                           int cornerSegs) {
    rx = std::min(rx, rect.w * 0.5f);
    ry = std::min(ry, rect.h * 0.5f);
    float cr = c.r / 255.0f, cg = c.g / 255.0f, cb = c.b / 255.0f,
          ca = c.a / 255.0f;
    float x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    float hw = width * 0.5f;

    // Top edge
    pushQuad(x + rx, y - hw, x + w - rx, y + hw, c);
    // Bottom edge
    pushQuad(x + rx, y + h - hw, x + w - rx, y + h + hw, c);
    // Left edge
    pushQuad(x - hw, y + ry, x + hw, y + h - ry, c);
    // Right edge
    pushQuad(x + w - hw, y + ry, x + w + hw, y + h - ry, c);

    // Corner arcs (ring segments)
    float step = (3.14159265f * 0.5f) / cornerSegs;
    struct Corner {
      float cx, cy;
      float startAngle;
    };
    Corner corners[4] = {{x + rx, y + ry, 3.14159265f},
                         {x + w - rx, y + ry, 3.14159265f * 1.5f},
                         {x + w - rx, y + h - ry, 0.0f},
                         {x + rx, y + h - ry, 3.14159265f * 0.5f}};
    float outerRx = rx + hw, outerRy = ry + hw;
    float innerRx = rx - hw, innerRy = ry - hw;
    if (innerRx < 0)
      innerRx = 0;
    if (innerRy < 0)
      innerRy = 0;
    for (auto &co : corners) {
      for (int i = 0; i < cornerSegs; ++i) {
        float a0 = co.startAngle + i * step;
        float a1 = co.startAngle + (i + 1) * step;
        float cos0 = std::cos(a0), sin0 = std::sin(a0);
        float cos1 = std::cos(a1), sin1 = std::sin(a1);
        float ox0 = co.cx + outerRx * cos0, oy0 = co.cy + outerRy * sin0;
        float ox1 = co.cx + outerRx * cos1, oy1 = co.cy + outerRy * sin1;
        float ix0 = co.cx + innerRx * cos0, iy0 = co.cy + innerRy * sin0;
        float ix1 = co.cx + innerRx * cos1, iy1 = co.cy + innerRy * sin1;
        colorVerts_.insert(colorVerts_.end(), {{ox0, oy0, cr, cg, cb, ca},
                                               {ix0, iy0, cr, cg, cb, ca},
                                               {ox1, oy1, cr, cg, cb, ca},
                                               {ix0, iy0, cr, cg, cb, ca},
                                               {ix1, iy1, cr, cg, cb, ca},
                                               {ox1, oy1, cr, cg, cb, ca}});
      }
    }
  }

  void flushColorBatch() {
    if (colorVerts_.empty())
      return;
    colorProgram_.use();
    colorProgram_.setProjection(float(framebuffer_.width),
                                float(framebuffer_.height));
    colorBuffer_.upload(colorVerts_);
    colorBuffer_.boundCount = GLsizei(colorVerts_.size());
    colorBuffer_.bind();
    glDrawArrays(GL_TRIANGLES, 0, colorBuffer_.boundCount);
    glBindVertexArray(0);
    colorVerts_.clear();
  }

  void flushTexBatch(GLuint tex) {
    if (texVerts_.empty())
      return;
    texProgram_.use();
    texProgram_.setProjection(float(framebuffer_.width),
                              float(framebuffer_.height));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(texProgram_.samplerLoc, 0);
    texBuffer_.upload(texVerts_);
    texBuffer_.boundCount = GLsizei(texVerts_.size());
    texBuffer_.bind();
    glDrawArrays(GL_TRIANGLES, 0, texBuffer_.boundCount);
    glBindVertexArray(0);
    texVerts_.clear();
  }
};

// Factory
namespace GpuContexts {

std::shared_ptr<GpuContext> MakeGL() {
  auto glImpl = std::make_shared<GLImpl>();
  if (!glImpl->init(1, 1))
    return nullptr;
  return MakeGpuContextFromImpl(glImpl);
}

} // namespace GpuContexts

} // namespace ink

#endif // INK_HAS_GL
