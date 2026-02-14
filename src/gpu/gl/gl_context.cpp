// GL context implementation - implements GpuImpl for OpenGL.
//
// Only compiled when INK_HAS_GL is defined (via CMake).
// Requires OpenGL 3.3+ core profile.

#include "ink/gpu/gl/gl_context.hpp"
#include "ink/gpu/gpu_context.hpp"
#include "ink/recording.hpp"
#include "ink/draw_pass.hpp"
#include "ink/image.hpp"
#include "ink/glyph_cache.hpp"
#include "gpu_impl.hpp"

#if INK_HAS_GL

#include <GL/glew.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <unordered_map>

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

// GL utilities
static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "ink GL: shader error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint linkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "ink GL: link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static GLuint createProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return 0;
    }
    GLuint prog = linkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

static void makeOrthoMatrix(float* m, float w, float h) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = 2.0f / w; m[5] = -2.0f / h; m[10] = -1.0f;
    m[12] = -1.0f; m[13] = 1.0f; m[15] = 1.0f;
}

struct ColorVertex { float x, y, r, g, b, a; };
struct TexVertex { float x, y, u, v; };

// GL implementation of GpuImpl
class GLImpl : public GpuImpl {
public:
    i32 width_ = 0, height_ = 0;
    GlyphCache* glyphCache_ = nullptr;
    GLuint fbo_ = 0, colorTexture_ = 0;
    GLuint colorProgram_ = 0, texProgram_ = 0;
    GLint colorProjLoc_ = -1, texProjLoc_ = -1, texSamplerLoc_ = -1;
    GLuint colorVAO_ = 0, colorVBO_ = 0, texVAO_ = 0, texVBO_ = 0;
    GLuint tempTexture_ = 0;
    std::vector<ColorVertex> colorVerts_;
    std::vector<TexVertex> texVerts_;
    std::unordered_map<u64, GLuint> cpuImageCache_;

    ~GLImpl() override { destroy(); }

    bool init(i32 w, i32 h) {
        width_ = w; height_ = h;
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

        colorProgram_ = createProgram(kColorVertSrc, kColorFragSrc);
        if (!colorProgram_) return false;
        colorProjLoc_ = glGetUniformLocation(colorProgram_, "uProjection");

        texProgram_ = createProgram(kTextureVertSrc, kTextureFragSrc);
        if (!texProgram_) return false;
        texProjLoc_ = glGetUniformLocation(texProgram_, "uProjection");
        texSamplerLoc_ = glGetUniformLocation(texProgram_, "uTexture");

        glGenVertexArrays(1, &colorVAO_);
        glGenBuffers(1, &colorVBO_);
        glBindVertexArray(colorVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, colorVBO_);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), (void*)8);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        glGenVertexArrays(1, &texVAO_);
        glGenBuffers(1, &texVBO_);
        glBindVertexArray(texVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, texVBO_);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TexVertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexVertex), (void*)8);
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);

        glGenTextures(1, &tempTexture_);
        createFBO(w, h);
        return true;
    }

    void createFBO(i32 w, i32 h) {
        if (colorTexture_) glDeleteTextures(1, &colorTexture_);
        if (fbo_) glDeleteFramebuffers(1, &fbo_);
        glGenTextures(1, &colorTexture_);
        glBindTexture(GL_TEXTURE_2D, colorTexture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void destroy() {
        for (auto& e : cpuImageCache_) if (e.second) glDeleteTextures(1, &e.second);
        cpuImageCache_.clear();
        if (colorTexture_) glDeleteTextures(1, &colorTexture_);
        if (fbo_) glDeleteFramebuffers(1, &fbo_);
        if (colorProgram_) glDeleteProgram(colorProgram_);
        if (texProgram_) glDeleteProgram(texProgram_);
        if (colorVAO_) glDeleteVertexArrays(1, &colorVAO_);
        if (colorVBO_) glDeleteBuffers(1, &colorVBO_);
        if (texVAO_) glDeleteVertexArrays(1, &texVAO_);
        if (texVBO_) glDeleteBuffers(1, &texVBO_);
        if (tempTexture_) glDeleteTextures(1, &tempTexture_);
    }

    // GpuImpl interface
    bool valid() const override { return fbo_ != 0; }

    void beginFrame() override {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width_, height_);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
    }

    void endFrame() override { flushColorBatch(); glFlush(); }

    void resize(i32 w, i32 h) override { width_ = w; height_ = h; createFBO(w, h); }

    void execute(const Recording& recording, const DrawPass& pass) override {
        const auto& ops = recording.ops();
        const auto& arena = recording.arena();
        DrawOp::Type lastType = DrawOp::Type(-1);

        for (u32 idx : pass.sortedIndices()) {
            const auto& op = ops[idx];
            if (op.type != lastType && lastType != DrawOp::Type(-1)) flushColorBatch();
            lastType = op.type;

            switch (op.type) {
            case DrawOp::Type::FillRect: {
                Rect r = op.data.fill.rect;
                pushQuad(r.x, r.y, r.x + r.w, r.y + r.h, op.color);
                break;
            }
            case DrawOp::Type::StrokeRect: {
                Rect r = op.data.stroke.rect;
                float w = op.width > 0 ? op.width : 1.0f;
                pushQuad(r.x, r.y, r.x + r.w, r.y + w, op.color);
                pushQuad(r.x, r.y + r.h - w, r.x + r.w, r.y + r.h, op.color);
                pushQuad(r.x, r.y + w, r.x + w, r.y + r.h - w, op.color);
                pushQuad(r.x + r.w - w, r.y + w, r.x + r.w, r.y + r.h - w, op.color);
                break;
            }
            case DrawOp::Type::Line: {
                pushLine(op.data.line.p1.x, op.data.line.p1.y, op.data.line.p2.x, op.data.line.p2.y, op.color, op.width > 0 ? op.width : 1.0f);
                break;
            }
            case DrawOp::Type::Polyline: {
                const Point* pts = arena.getPoints(op.data.polyline.offset);
                i32 count = i32(op.data.polyline.count);
                float w = op.width > 0 ? op.width : 1.0f;
                for (i32 i = 0; i + 1 < count; ++i) pushLine(pts[i].x, pts[i].y, pts[i+1].x, pts[i+1].y, op.color, w);
                break;
            }
            case DrawOp::Type::Text: {
                flushColorBatch();
                if (glyphCache_) {
                    const char* text = arena.getString(op.data.text.offset);
                    i32 len = i32(op.data.text.len);
                    i32 tw = glyphCache_->measureText(std::string_view(text, len));
                    i32 th = glyphCache_->lineHeight();
                    if (tw > 0 && th > 0) {
                        std::vector<u32> buf(tw * th, 0);
                        glyphCache_->drawText(buf.data(), tw, th, 0, 0, std::string_view(text, len), op.color);
                        glBindTexture(GL_TEXTURE_2D, tempTexture_);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tw, th, 0, GL_BGRA, GL_UNSIGNED_BYTE, buf.data());
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        float x = op.data.text.pos.x, y = op.data.text.pos.y - th;
                        pushTexQuad(x, y, x + tw, y + th, 0, 0, 1, 1);
                        flushTexBatch(tempTexture_);
                    }
                }
                break;
            }
            case DrawOp::Type::DrawImage: {
                flushColorBatch();
                const Image* image = recording.getImage(op.data.image.imageIndex);
                if (image && image->valid()) {
                    GLuint tex = static_cast<GLuint>(resolveImageTexture(image));
                    if (tex) {
                        float x = op.data.image.x, y = op.data.image.y;
                        float w = float(image->width()), h = float(image->height());
                        pushTexQuad(x, y, x + w, y + h, 0, 0, 1, 1);
                        flushTexBatch(tex);
                    }
                }
                break;
            }
            case DrawOp::Type::SetClip: {
                flushColorBatch();
                Rect r = op.data.clip.rect;
                glEnable(GL_SCISSOR_TEST);
                glScissor(GLint(r.x), height_ - GLint(r.y + r.h), GLsizei(r.w), GLsizei(r.h));
                break;
            }
            case DrawOp::Type::ClearClip: {
                flushColorBatch();
                glDisable(GL_SCISSOR_TEST);
                break;
            }
            }
        }
        flushColorBatch();
    }

    std::shared_ptr<Image> makeSnapshot() const override {
        if (width_ <= 0 || height_ <= 0) return nullptr;
        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (!tex) return nullptr;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GLuint dstFbo = 0;
        glGenFramebuffers(1, &dstFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
        glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glDeleteFramebuffers(1, &dstFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        auto owner = std::shared_ptr<GLuint>(new GLuint(tex), [](GLuint* t) { if (t && *t) glDeleteTextures(1, t); delete t; });
        return Image::MakeFromBackendTexture(tex, width_, height_, PixelFormat::RGBA8888, owner);
    }

    void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const override {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    }

    unsigned int textureId() const override { return colorTexture_; }
    unsigned int fboId() const override { return fbo_; }

    u64 resolveImageTexture(const Image* image) override {
        if (!image || !image->valid()) return 0;
        if (image->isGpuBacked()) return image->backendTextureHandle();
        u64 id = image->uniqueId();
        auto it = cpuImageCache_.find(id);
        if (it != cpuImageCache_.end()) return it->second;
        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (!tex) return 0;
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->width(), image->height(), 0,
                     image->format() == PixelFormat::BGRA8888 ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE, image->pixels());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        cpuImageCache_[id] = tex;
        return tex;
    }

    // Batching
    void pushQuad(float x0, float y0, float x1, float y1, Color c) {
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        colorVerts_.insert(colorVerts_.end(), {{x0,y0,r,g,b,a},{x1,y0,r,g,b,a},{x0,y1,r,g,b,a},{x1,y0,r,g,b,a},{x1,y1,r,g,b,a},{x0,y1,r,g,b,a}});
    }

    void pushLine(float x0, float y0, float x1, float y1, Color c, float width) {
        float dx = x1-x0, dy = y1-y0, len = std::sqrt(dx*dx+dy*dy);
        if (len < 0.0001f) return;
        float hw = width*0.5f, nx = -dy/len*hw, ny = dx/len*hw;
        float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f, a = c.a/255.0f;
        colorVerts_.insert(colorVerts_.end(), {{x0+nx,y0+ny,r,g,b,a},{x0-nx,y0-ny,r,g,b,a},{x1+nx,y1+ny,r,g,b,a},{x0-nx,y0-ny,r,g,b,a},{x1-nx,y1-ny,r,g,b,a},{x1+nx,y1+ny,r,g,b,a}});
    }

    void pushTexQuad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
        texVerts_.insert(texVerts_.end(), {{x0,y0,u0,v0},{x1,y0,u1,v0},{x0,y1,u0,v1},{x1,y0,u1,v0},{x1,y1,u1,v1},{x0,y1,u0,v1}});
    }

    void flushColorBatch() {
        if (colorVerts_.empty()) return;
        glUseProgram(colorProgram_);
        float proj[16]; makeOrthoMatrix(proj, float(width_), float(height_));
        glUniformMatrix4fv(colorProjLoc_, 1, GL_FALSE, proj);
        glBindVertexArray(colorVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, colorVBO_);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(colorVerts_.size()*sizeof(ColorVertex)), colorVerts_.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(colorVerts_.size()));
        glBindVertexArray(0);
        colorVerts_.clear();
    }

    void flushTexBatch(GLuint tex) {
        if (texVerts_.empty()) return;
        glUseProgram(texProgram_);
        float proj[16]; makeOrthoMatrix(proj, float(width_), float(height_));
        glUniformMatrix4fv(texProjLoc_, 1, GL_FALSE, proj);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(texSamplerLoc_, 0);
        glBindVertexArray(texVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, texVBO_);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(texVerts_.size()*sizeof(TexVertex)), texVerts_.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, GLsizei(texVerts_.size()));
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
