// GL context implementation - implements GpuImpl for OpenGL.
//
// Only compiled when INK_HAS_GL is defined (via CMake).
// Requires OpenGL 3.3+ core profile.

#include "ink/gpu/gl/gl_context.hpp"
#include "ink/gpu/gpu_context.hpp"
#include "ink/recording.hpp"
#include "ink/draw_pass.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/image.hpp"
#include "ink/paint.hpp"
#include "ink/glyph_cache.hpp"
#include "gpu_impl.hpp"
#include "gl_resources.hpp"

#if INK_HAS_GL

#include <vector>
#include <cstdio>
#include <cmath>

namespace ink {

// Shader sources
static const char* kColorVertSrc = R"(
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

static const char* kColorFragSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

static const char* kTextureVertSrc = R"(
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

static const char* kTextureFragSrc = R"(
#version 330 core
in vec2 vTexCoord;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(uTexture, vTexCoord);
}
)";

// GLTextureCache::resolve implementation
GLuint GLTextureCache::resolve(const Image* image) {
    if (!image || !image->valid()) return 0;
    if (image->isGpuBacked()) return static_cast<GLuint>(image->backendTextureHandle());

    u64 id = image->uniqueId();
    auto it = cache_.find(id);
    if (it != cache_.end()) return it->second;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) return 0;

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width(), image->height(), 0,
                 image->format() == PixelFormat::BGRA8888 ? GL_BGRA : GL_RGBA,
                 GL_UNSIGNED_BYTE, image->pixels());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    cache_[id] = tex;
    return tex;
}

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
    GlyphCache* glyphCache_ = nullptr;
    std::vector<GLColorVertex> colorVerts_;
    std::vector<GLTexVertex> texVerts_;
    const Recording* currentRecording_ = nullptr;

    ~GLImpl() override { destroy(); }

    bool init(i32 w, i32 h) {
        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
        while (glGetError() != GL_NO_ERROR) {}
        if (err != GLEW_OK && glGetString(GL_VERSION) == nullptr) {
            std::fprintf(stderr, "ink GL: GLEW init failed\n");
            return false;
        }
        if (glGetString(GL_VERSION) == nullptr) {
            std::fprintf(stderr, "ink GL: no GL context\n");
            return false;
        }

        if (!colorProgram_.init(kColorVertSrc, kColorFragSrc)) return false;
        colorProgram_.projLoc = glGetUniformLocation(colorProgram_.program, "uProjection");

        if (!texProgram_.init(kTextureVertSrc, kTextureFragSrc)) return false;
        texProgram_.projLoc = glGetUniformLocation(texProgram_.program, "uProjection");
        texProgram_.samplerLoc = glGetUniformLocation(texProgram_.program, "uTexture");

        // Color VAO with position (2 float) + color (4 float)
        colorBuffer_.init();
        glBindVertexArray(colorBuffer_.vao);
        glBindBuffer(GL_ARRAY_BUFFER, colorBuffer_.vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLColorVertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLColorVertex), (void*)8);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        // Texture VAO with position (2 float) + uv (2 float)
        texBuffer_.init();
        glBindVertexArray(texBuffer_.vao);
        glBindBuffer(GL_ARRAY_BUFFER, texBuffer_.vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLTexVertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLTexVertex), (void*)8);
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
        if (tempTexture_) { glDeleteTextures(1, &tempTexture_); tempTexture_ = 0; }
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
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
    }

    void endFrame() override {
        flushColorBatch();
        glFlush();
    }

    void resize(i32 w, i32 h) override {
        framebuffer_.resize(w, h);
    }

    void setGlyphCache(GlyphCache* cache) override { glyphCache_ = cache; }

    void execute(const Recording& recording, const DrawPass& pass) override {
        currentRecording_ = &recording;
        recording.dispatch(*this, pass);
        flushColorBatch();
        currentRecording_ = nullptr;
    }

    // DrawOpVisitor interface
    void visitFillRect(Rect r, Color c, BlendMode, u8) override {
        pushQuad(r.x, r.y, r.x + r.w, r.y + r.h, c);
    }

    void visitStrokeRect(Rect r, Color c, f32 width, BlendMode, u8) override {
        float w = width > 0 ? width : 1.0f;
        pushQuad(r.x, r.y, r.x + r.w, r.y + w, c);
        pushQuad(r.x, r.y + r.h - w, r.x + r.w, r.y + r.h, c);
        pushQuad(r.x, r.y + w, r.x + w, r.y + r.h - w, c);
        pushQuad(r.x + r.w - w, r.y + w, r.x + r.w, r.y + r.h - w, c);
    }

    void visitLine(Point p1, Point p2, Color c, f32 width, BlendMode, u8) override {
        pushLine(p1.x, p1.y, p2.x, p2.y, c, width > 0 ? width : 1.0f);
    }

    void visitPolyline(const Point* pts, i32 count, Color c, f32 width, BlendMode, u8) override {
        float w = width > 0 ? width : 1.0f;
        for (i32 i = 0; i + 1 < count; ++i)
            pushLine(pts[i].x, pts[i].y, pts[i+1].x, pts[i+1].y, c, w);
    }

    void visitText(Point p, const char* text, u32 len, Color c, BlendMode, u8) override {
        flushColorBatch();
        if (glyphCache_) {
            i32 tw = glyphCache_->measureText(std::string_view(text, len));
            i32 th = glyphCache_->lineHeight();
            if (tw > 0 && th > 0) {
                std::vector<u32> buf(tw * th, 0);
                glyphCache_->drawText(buf.data(), tw, th, 0, 0, std::string_view(text, len), c);
                glBindTexture(GL_TEXTURE_2D, tempTexture_);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tw, th, 0, GL_BGRA, GL_UNSIGNED_BYTE, buf.data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                float x = p.x, y = p.y - th;
                pushTexQuad(x, y, x + tw, y + th, 0, 0, 1, 1);
                flushTexBatch(tempTexture_);
            }
        }
    }

    void visitDrawImage(const Image* image, f32 x, f32 y, BlendMode, u8) override {
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
        glScissor(GLint(r.x), framebuffer_.height - GLint(r.y + r.h), GLsizei(r.w), GLsizei(r.h));
    }

    void visitClearClip() override {
        flushColorBatch();
        glDisable(GL_SCISSOR_TEST);
    }

    void visitSetTransform(const Matrix&) override {
        flushColorBatch();
    }

    void visitClearTransform() override {
        flushColorBatch();
    }

    void visitFillCircle(f32 cx, f32 cy, f32 radius, Color c, BlendMode, u8) override {
        pushCircleFan(cx, cy, radius, c, 48);
    }

    void visitStrokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width, BlendMode, u8) override {
        pushCircleRing(cx, cy, radius, c, width > 0 ? width : 1.0f, 48);
    }

    void visitFillRoundRect(Rect r, f32 rx, f32 ry, Color c, BlendMode, u8) override {
        pushRoundRectFill(r, rx, ry, c, 16);
    }

    void visitStrokeRoundRect(Rect r, f32 rx, f32 ry, Color c, f32 width, BlendMode, u8) override {
        pushRoundRectStroke(r, rx, ry, c, width > 0 ? width : 1.0f, 16);
    }

    std::shared_ptr<Image> makeSnapshot() const override {
        if (framebuffer_.width <= 0 || framebuffer_.height <= 0) return nullptr;
        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (!tex) return nullptr;

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebuffer_.width, framebuffer_.height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        GLuint dstFbo = 0;
        glGenFramebuffers(1, &dstFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_.fbo);
        glBlitFramebuffer(0, 0, framebuffer_.width, framebuffer_.height,
                          0, 0, framebuffer_.width, framebuffer_.height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glDeleteFramebuffers(1, &dstFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.fbo);

        auto owner = std::shared_ptr<GLuint>(new GLuint(tex), [](GLuint* t) { if (t && *t) glDeleteTextures(1, t); delete t; });
        return Image::MakeFromBackendTexture(tex, framebuffer_.width, framebuffer_.height, PixelFormat::RGBA8888, owner);
    }

    void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const override {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.fbo);
        glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    }

    unsigned int glTextureId() const override { return framebuffer_.texture; }
    unsigned int glFboId() const override { return framebuffer_.fbo; }
    
    GLInterop* glInterop() override { return this; }

    u64 resolveImageTexture(const Image* image) override {
        return textureCache_.resolve(image);
    }

    // Batching
    void pushQuad(float x0, float y0, float x1, float y1, Color c) {
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        colorVerts_.insert(colorVerts_.end(), {
            {x0,y0,r,g,b,a}, {x1,y0,r,g,b,a}, {x0,y1,r,g,b,a},
            {x1,y0,r,g,b,a}, {x1,y1,r,g,b,a}, {x0,y1,r,g,b,a}
        });
    }

    void pushLine(float x0, float y0, float x1, float y1, Color c, float width) {
        float dx = x1-x0, dy = y1-y0, len = std::sqrt(dx*dx + dy*dy);
        if (len < 0.0001f) return;
        float hw = width * 0.5f, nx = -dy/len*hw, ny = dx/len*hw;
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        colorVerts_.insert(colorVerts_.end(), {
            {x0+nx,y0+ny,r,g,b,a}, {x0-nx,y0-ny,r,g,b,a}, {x1+nx,y1+ny,r,g,b,a},
            {x0-nx,y0-ny,r,g,b,a}, {x1-nx,y1-ny,r,g,b,a}, {x1+nx,y1+ny,r,g,b,a}
        });
    }

    void pushTexQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
        texVerts_.insert(texVerts_.end(), {
            {x0,y0,u0,v0}, {x1,y0,u1,v0}, {x0,y1,u0,v1},
            {x1,y0,u1,v0}, {x1,y1,u1,v1}, {x0,y1,u0,v1}
        });
    }

    void pushCircleFan(float cx, float cy, float radius, Color c, int segments) {
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        float step = 2.0f * 3.14159265f / segments;
        for (int i = 0; i < segments; ++i) {
            float a0 = i * step, a1 = (i + 1) * step;
            float x0 = cx + radius * std::cos(a0), y0 = cy + radius * std::sin(a0);
            float x1 = cx + radius * std::cos(a1), y1 = cy + radius * std::sin(a1);
            colorVerts_.insert(colorVerts_.end(), {
                {cx, cy, r, g, b, a}, {x0, y0, r, g, b, a}, {x1, y1, r, g, b, a}
            });
        }
    }

    void pushCircleRing(float cx, float cy, float radius, Color c, float width, int segments) {
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        float hw = width * 0.5f;
        float outerR = radius + hw, innerR = radius - hw;
        if (innerR < 0) innerR = 0;
        float step = 2.0f * 3.14159265f / segments;
        for (int i = 0; i < segments; ++i) {
            float a0 = i * step, a1 = (i + 1) * step;
            float cos0 = std::cos(a0), sin0 = std::sin(a0);
            float cos1 = std::cos(a1), sin1 = std::sin(a1);
            float ox0 = cx + outerR * cos0, oy0 = cy + outerR * sin0;
            float ox1 = cx + outerR * cos1, oy1 = cy + outerR * sin1;
            float ix0 = cx + innerR * cos0, iy0 = cy + innerR * sin0;
            float ix1 = cx + innerR * cos1, iy1 = cy + innerR * sin1;
            colorVerts_.insert(colorVerts_.end(), {
                {ox0, oy0, r, g, b, a}, {ix0, iy0, r, g, b, a}, {ox1, oy1, r, g, b, a},
                {ix0, iy0, r, g, b, a}, {ix1, iy1, r, g, b, a}, {ox1, oy1, r, g, b, a}
            });
        }
    }

    void pushRoundRectFill(Rect rect, float rx, float ry, Color c, int cornerSegs) {
        rx = std::min(rx, rect.w * 0.5f);
        ry = std::min(ry, rect.h * 0.5f);
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        float x = rect.x, y = rect.y, w = rect.w, h = rect.h;

        // Center cross (two rectangles)
        pushQuad(x + rx, y, x + w - rx, y + h, c);  // vertical strip
        pushQuad(x, y + ry, x + rx, y + h - ry, c);  // left strip
        pushQuad(x + w - rx, y + ry, x + w, y + h - ry, c);  // right strip

        // Four corner fans
        float step = (3.14159265f * 0.5f) / cornerSegs;
        // Corners: TL, TR, BR, BL with center offsets and angle starts
        struct Corner { float cx, cy; float startAngle; };
        Corner corners[4] = {
            {x + rx,     y + ry,     3.14159265f},        // top-left
            {x + w - rx, y + ry,     3.14159265f * 1.5f}, // top-right
            {x + w - rx, y + h - ry, 0.0f},               // bottom-right
            {x + rx,     y + h - ry, 3.14159265f * 0.5f}  // bottom-left
        };
        for (auto& co : corners) {
            for (int i = 0; i < cornerSegs; ++i) {
                float a0 = co.startAngle + i * step;
                float a1 = co.startAngle + (i + 1) * step;
                float px0 = co.cx + rx * std::cos(a0), py0 = co.cy + ry * std::sin(a0);
                float px1 = co.cx + rx * std::cos(a1), py1 = co.cy + ry * std::sin(a1);
                colorVerts_.insert(colorVerts_.end(), {
                    {co.cx, co.cy, r, g, b, a}, {px0, py0, r, g, b, a}, {px1, py1, r, g, b, a}
                });
            }
        }
    }

    void pushRoundRectStroke(Rect rect, float rx, float ry, Color c, float width, int cornerSegs) {
        rx = std::min(rx, rect.w * 0.5f);
        ry = std::min(ry, rect.h * 0.5f);
        float cr = c.r/255.0f, cg = c.g/255.0f, cb = c.b/255.0f, ca = c.a/255.0f;
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
        struct Corner { float cx, cy; float startAngle; };
        Corner corners[4] = {
            {x + rx,     y + ry,     3.14159265f},
            {x + w - rx, y + ry,     3.14159265f * 1.5f},
            {x + w - rx, y + h - ry, 0.0f},
            {x + rx,     y + h - ry, 3.14159265f * 0.5f}
        };
        float outerRx = rx + hw, outerRy = ry + hw;
        float innerRx = rx - hw, innerRy = ry - hw;
        if (innerRx < 0) innerRx = 0;
        if (innerRy < 0) innerRy = 0;
        for (auto& co : corners) {
            for (int i = 0; i < cornerSegs; ++i) {
                float a0 = co.startAngle + i * step;
                float a1 = co.startAngle + (i + 1) * step;
                float cos0 = std::cos(a0), sin0 = std::sin(a0);
                float cos1 = std::cos(a1), sin1 = std::sin(a1);
                float ox0 = co.cx + outerRx * cos0, oy0 = co.cy + outerRy * sin0;
                float ox1 = co.cx + outerRx * cos1, oy1 = co.cy + outerRy * sin1;
                float ix0 = co.cx + innerRx * cos0, iy0 = co.cy + innerRy * sin0;
                float ix1 = co.cx + innerRx * cos1, iy1 = co.cy + innerRy * sin1;
                colorVerts_.insert(colorVerts_.end(), {
                    {ox0, oy0, cr, cg, cb, ca}, {ix0, iy0, cr, cg, cb, ca}, {ox1, oy1, cr, cg, cb, ca},
                    {ix0, iy0, cr, cg, cb, ca}, {ix1, iy1, cr, cg, cb, ca}, {ox1, oy1, cr, cg, cb, ca}
                });
            }
        }
    }

    void flushColorBatch() {
        if (colorVerts_.empty()) return;
        colorProgram_.use();
        colorProgram_.setProjection(float(framebuffer_.width), float(framebuffer_.height));
        colorBuffer_.upload(colorVerts_);
        colorBuffer_.boundCount = GLsizei(colorVerts_.size());
        colorBuffer_.bind();
        glDrawArrays(GL_TRIANGLES, 0, colorBuffer_.boundCount);
        glBindVertexArray(0);
        colorVerts_.clear();
    }

    void flushTexBatch(GLuint tex) {
        if (texVerts_.empty()) return;
        texProgram_.use();
        texProgram_.setProjection(float(framebuffer_.width), float(framebuffer_.height));
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
    if (!glImpl->init(1, 1)) return nullptr;
    return MakeGpuContextFromImpl(glImpl);
}

} // namespace GpuContexts

} // namespace ink

#endif // INK_HAS_GL
