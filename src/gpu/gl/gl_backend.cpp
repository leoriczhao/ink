// GLBackend - Full OpenGL rendering backend implementation
//
// This file is only compiled when INK_HAS_GL is defined (via CMake).
// Requires OpenGL 3.3+ core profile.

#include "ink/gpu/gl_backend.hpp"
#include "ink/gpu/gpu_context.hpp"

#if INK_HAS_GL

#include "ink/recording.hpp"
#include "ink/draw_pass.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/image.hpp"
#include "ink/glyph_cache.hpp"

#include <GL/glew.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace ink {

// ============================================================
// Shader sources
// ============================================================

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

// ============================================================
// GL utility functions
// ============================================================

static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "ink GLBackend: shader compile error: %s\n", log);
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
        std::fprintf(stderr, "ink GLBackend: program link error: %s\n", log);
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
    // Column-major orthographic projection: (0,0) top-left, (w,h) bottom-right
    std::memset(m, 0, 16 * sizeof(float));
    m[0]  =  2.0f / w;
    m[5]  = -2.0f / h;   // flip Y: top=0
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] =  1.0f;
    m[14] =  0.0f;
    m[15] =  1.0f;
}

// ============================================================
// Vertex types
// ============================================================

struct ColorVertex {
    float x, y;
    float r, g, b, a;
};

struct TexVertex {
    float x, y;
    float u, v;
};

// ============================================================
// GLBackend::Impl
// ============================================================

struct GLBackend::Impl {
    i32 width = 0;
    i32 height = 0;
    bool useDefaultFBO = false;
    GlyphCache* glyphCache = nullptr;

    // FBO (offscreen mode)
    GLuint fbo = 0;
    GLuint colorTexture = 0;

    // Color pipeline (FillRect, StrokeRect, Line, Polyline)
    GLuint colorProgram = 0;
    GLint  colorProjLoc = -1;
    GLuint colorVAO = 0;
    GLuint colorVBO = 0;

    // Texture pipeline (DrawImage)
    GLuint texProgram = 0;
    GLint  texProjLoc = -1;
    GLint  texSamplerLoc = -1;
    GLuint texVAO = 0;
    GLuint texVBO = 0;

    // Batched vertices
    std::vector<ColorVertex> colorVerts;
    std::vector<TexVertex> texVerts;
    GLenum currentPrimitive = GL_TRIANGLES;

    // Texture cache for drawImage
    GLuint tempTexture = 0;

    bool init(i32 w, i32 h, bool defaultFBO);
    void destroy();
    void createFBO(i32 w, i32 h);
    void destroyFBO();
    std::shared_ptr<Image> makeSnapshotImage() const;

    void flushColorBatch();
    void flushTexBatch(GLuint texId);

    void pushQuad(float x0, float y0, float x1, float y1, Color c);
    void pushLine(float x0, float y0, float x1, float y1, Color c, float width);
    void pushTexQuad(float x0, float y0, float x1, float y1,
                     float u0, float v0, float u1, float v1);
};

bool GLBackend::Impl::init(i32 w, i32 h, bool defaultFBO) {
    width = w;
    height = h;
    useDefaultFBO = defaultFBO;

    // Create shader programs
    colorProgram = createProgram(kColorVertSrc, kColorFragSrc);
    if (!colorProgram) return false;
    colorProjLoc = glGetUniformLocation(colorProgram, "uProjection");

    texProgram = createProgram(kTextureVertSrc, kTextureFragSrc);
    if (!texProgram) return false;
    texProjLoc = glGetUniformLocation(texProgram, "uProjection");
    texSamplerLoc = glGetUniformLocation(texProgram, "uTexture");

    // Color VAO/VBO
    glGenVertexArrays(1, &colorVAO);
    glGenBuffers(1, &colorVBO);
    glBindVertexArray(colorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    // pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex),
                          (void*)offsetof(ColorVertex, x));
    glEnableVertexAttribArray(0);
    // color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex),
                          (void*)offsetof(ColorVertex, r));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Texture VAO/VBO
    glGenVertexArrays(1, &texVAO);
    glGenBuffers(1, &texVBO);
    glBindVertexArray(texVAO);
    glBindBuffer(GL_ARRAY_BUFFER, texVBO);
    // pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TexVertex),
                          (void*)offsetof(TexVertex, x));
    glEnableVertexAttribArray(0);
    // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexVertex),
                          (void*)offsetof(TexVertex, u));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Temp texture for drawImage
    glGenTextures(1, &tempTexture);

    // FBO
    if (!useDefaultFBO) {
        createFBO(w, h);
    }

    return true;
}

void GLBackend::Impl::createFBO(i32 w, i32 h) {
    if (colorTexture) {
        glDeleteTextures(1, &colorTexture);
        colorTexture = 0;
    }
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }

    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorTexture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "ink GLBackend: FBO incomplete: 0x%x\n", status);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLBackend::Impl::destroyFBO() {
    if (colorTexture) { glDeleteTextures(1, &colorTexture); colorTexture = 0; }
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
}

void GLBackend::Impl::destroy() {
    destroyFBO();
    if (colorProgram) { glDeleteProgram(colorProgram); colorProgram = 0; }
    if (texProgram) { glDeleteProgram(texProgram); texProgram = 0; }
    if (colorVAO) { glDeleteVertexArrays(1, &colorVAO); colorVAO = 0; }
    if (colorVBO) { glDeleteBuffers(1, &colorVBO); colorVBO = 0; }
    if (texVAO) { glDeleteVertexArrays(1, &texVAO); texVAO = 0; }
    if (texVBO) { glDeleteBuffers(1, &texVBO); texVBO = 0; }
    if (tempTexture) { glDeleteTextures(1, &tempTexture); tempTexture = 0; }
}

std::shared_ptr<Image> GLBackend::Impl::makeSnapshotImage() const {
    if (width <= 0 || height <= 0) return nullptr;

    GLuint snapshotTexture = 0;
    glGenTextures(1, &snapshotTexture);
    if (snapshotTexture == 0) return nullptr;

    glBindTexture(GL_TEXTURE_2D, snapshotTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint dstFbo = 0;
    glGenFramebuffers(1, &dstFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, snapshotTexture, 0);

    GLuint srcFbo = useDefaultFBO ? 0 : fbo;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);

    glBlitFramebuffer(0, 0, width, height,
                      0, 0, width, height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glDeleteFramebuffers(1, &dstFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, srcFbo);

    auto textureOwner = std::shared_ptr<GLuint>(new GLuint(snapshotTexture),
        [](GLuint* tex) {
            if (tex && *tex) {
                glDeleteTextures(1, tex);
            }
            delete tex;
        });

    return Image::MakeFromBackendTexture(static_cast<u64>(snapshotTexture),
                                    width,
                                    height,
                                    PixelFormat::RGBA8888,
                                    textureOwner);
}

// ============================================================
// Batching helpers
// ============================================================

void GLBackend::Impl::pushQuad(float x0, float y0, float x1, float y1, Color c) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f, a = c.a / 255.0f;
    // Two triangles
    colorVerts.push_back({x0, y0, r, g, b, a});
    colorVerts.push_back({x1, y0, r, g, b, a});
    colorVerts.push_back({x0, y1, r, g, b, a});
    colorVerts.push_back({x1, y0, r, g, b, a});
    colorVerts.push_back({x1, y1, r, g, b, a});
    colorVerts.push_back({x0, y1, r, g, b, a});
}

void GLBackend::Impl::pushLine(float x0, float y0, float x1, float y1,
                                Color c, float width) {
    // Expand line into a quad perpendicular to the line direction
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.0001f) return;

    float hw = width * 0.5f;
    float nx = -dy / len * hw;  // perpendicular normal
    float ny =  dx / len * hw;

    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f, a = c.a / 255.0f;

    // Quad: 4 corners, 2 triangles
    ColorVertex v0 = {x0 + nx, y0 + ny, r, g, b, a};
    ColorVertex v1 = {x0 - nx, y0 - ny, r, g, b, a};
    ColorVertex v2 = {x1 + nx, y1 + ny, r, g, b, a};
    ColorVertex v3 = {x1 - nx, y1 - ny, r, g, b, a};

    colorVerts.push_back(v0);
    colorVerts.push_back(v1);
    colorVerts.push_back(v2);
    colorVerts.push_back(v1);
    colorVerts.push_back(v3);
    colorVerts.push_back(v2);
}

void GLBackend::Impl::pushTexQuad(float x0, float y0, float x1, float y1,
                                   float u0, float v0, float u1, float v1) {
    texVerts.push_back({x0, y0, u0, v0});
    texVerts.push_back({x1, y0, u1, v0});
    texVerts.push_back({x0, y1, u0, v1});
    texVerts.push_back({x1, y0, u1, v0});
    texVerts.push_back({x1, y1, u1, v1});
    texVerts.push_back({x0, y1, u0, v1});
}

void GLBackend::Impl::flushColorBatch() {
    if (colorVerts.empty()) return;

    glUseProgram(colorProgram);

    float proj[16];
    makeOrthoMatrix(proj, float(width), float(height));
    glUniformMatrix4fv(colorProjLoc, 1, GL_FALSE, proj);

    glBindVertexArray(colorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, colorVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 GLsizeiptr(colorVerts.size() * sizeof(ColorVertex)),
                 colorVerts.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, GLsizei(colorVerts.size()));

    glBindVertexArray(0);
    colorVerts.clear();
}

void GLBackend::Impl::flushTexBatch(GLuint texId) {
    if (texVerts.empty()) return;

    glUseProgram(texProgram);

    float proj[16];
    makeOrthoMatrix(proj, float(width), float(height));
    glUniformMatrix4fv(texProjLoc, 1, GL_FALSE, proj);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);
    glUniform1i(texSamplerLoc, 0);

    glBindVertexArray(texVAO);
    glBindBuffer(GL_ARRAY_BUFFER, texVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 GLsizeiptr(texVerts.size() * sizeof(TexVertex)),
                 texVerts.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, GLsizei(texVerts.size()));

    glBindVertexArray(0);
    texVerts.clear();
}

// ============================================================
// GLBackend public API
// ============================================================

GLBackend::GLBackend(std::shared_ptr<GpuContext> gpuContext,
                     i32 w,
                     i32 h,
                     bool useDefaultFBO)
    : impl_(std::make_unique<Impl>()),
      gpuContext_(std::move(gpuContext)) {
    impl_->init(w, h, useDefaultFBO);
}

GLBackend::~GLBackend() {
    if (impl_) impl_->destroy();
}

std::unique_ptr<Backend> GLBackend::Make(std::shared_ptr<GpuContext> gpuContext, i32 w, i32 h) {
    if (!gpuContext || !gpuContext->valid()) return nullptr;
    return std::unique_ptr<Backend>(new GLBackend(std::move(gpuContext), w, h, false));
}

std::unique_ptr<Backend> GLBackend::Make(i32 w, i32 h) {
    return Make(GpuContext::MakeGL(), w, h);
}

std::unique_ptr<Backend> GLBackend::MakeDefault(std::shared_ptr<GpuContext> gpuContext,
                                                i32 w,
                                                i32 h) {
    if (!gpuContext || !gpuContext->valid()) return nullptr;
    return std::unique_ptr<Backend>(new GLBackend(std::move(gpuContext), w, h, true));
}

std::unique_ptr<Backend> GLBackend::MakeDefault(i32 w, i32 h) {
    return MakeDefault(GpuContext::MakeGL(), w, h);
}

void GLBackend::beginFrame() {
    GLuint targetFBO = impl_->useDefaultFBO ? 0 : impl_->fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glViewport(0, 0, impl_->width, impl_->height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
}

void GLBackend::endFrame() {
    // Flush any remaining batched vertices
    impl_->flushColorBatch();
    glFlush();
}

void GLBackend::resize(i32 w, i32 h) {
    impl_->width = w;
    impl_->height = h;
    if (!impl_->useDefaultFBO) {
        impl_->createFBO(w, h);
    }
}

void GLBackend::setGlyphCache(GlyphCache* cache) {
    impl_->glyphCache = cache;
}

std::shared_ptr<Image> GLBackend::makeSnapshot() const {
    return impl_->makeSnapshotImage();
}

void GLBackend::execute(const Recording& recording, const DrawPass& pass) {
    const auto& ops = recording.ops();
    const auto& arena = recording.arena();

    DrawOp::Type lastType = DrawOp::Type(-1);

    for (u32 idx : pass.sortedIndices()) {
        const auto& op = ops[idx];

        // Flush batch on type change (to minimize state switches)
        if (op.type != lastType && lastType != DrawOp::Type(-1)) {
            impl_->flushColorBatch();
        }
        lastType = op.type;

        switch (op.type) {
        case DrawOp::Type::FillRect: {
            Rect r = op.data.fill.rect;
            impl_->pushQuad(r.x, r.y, r.x + r.w, r.y + r.h, op.color);
            break;
        }
        case DrawOp::Type::StrokeRect: {
            Rect r = op.data.stroke.rect;
            float w = op.width > 0 ? op.width : 1.0f;
            // Top
            impl_->pushQuad(r.x, r.y, r.x + r.w, r.y + w, op.color);
            // Bottom
            impl_->pushQuad(r.x, r.y + r.h - w, r.x + r.w, r.y + r.h, op.color);
            // Left
            impl_->pushQuad(r.x, r.y + w, r.x + w, r.y + r.h - w, op.color);
            // Right
            impl_->pushQuad(r.x + r.w - w, r.y + w, r.x + r.w, r.y + r.h - w, op.color);
            break;
        }
        case DrawOp::Type::Line: {
            impl_->pushLine(op.data.line.p1.x, op.data.line.p1.y,
                            op.data.line.p2.x, op.data.line.p2.y,
                            op.color, op.width > 0 ? op.width : 1.0f);
            break;
        }
        case DrawOp::Type::Polyline: {
            const Point* pts = arena.getPoints(op.data.polyline.offset);
            i32 count = i32(op.data.polyline.count);
            float w = op.width > 0 ? op.width : 1.0f;
            for (i32 i = 0; i + 1 < count; ++i) {
                impl_->pushLine(pts[i].x, pts[i].y,
                                pts[i + 1].x, pts[i + 1].y,
                                op.color, w);
            }
            break;
        }
        case DrawOp::Type::Text: {
            // Text rendering on GPU requires glyph atlas texture.
            // For now, fall back: flush color batch, render text to a temp
            // pixmap via GlyphCache, upload as texture.
            impl_->flushColorBatch();

            if (impl_->glyphCache) {
                // Get text metrics to determine size
                const char* text = arena.getString(op.data.text.offset);
                i32 textLen = i32(op.data.text.len);
                i32 textW = impl_->glyphCache->measureText(
                    std::string_view(text, textLen));
                i32 textH = impl_->glyphCache->lineHeight();

                if (textW > 0 && textH > 0) {
                    // Render text to a temporary RGBA buffer
                    std::vector<u32> buf(textW * textH, 0);
                    impl_->glyphCache->drawText(
                        buf.data(), textW, textH,
                        0, 0,
                        std::string_view(text, textLen), op.color);

                    // Upload to temp texture
                    glBindTexture(GL_TEXTURE_2D, impl_->tempTexture);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, textW, textH, 0,
                                 GL_BGRA, GL_UNSIGNED_BYTE, buf.data());
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

                    float x = op.data.text.pos.x;
                    float y = op.data.text.pos.y - textH; // baseline adjustment
                    impl_->pushTexQuad(x, y, x + textW, y + textH,
                                       0.0f, 0.0f, 1.0f, 1.0f);
                    impl_->flushTexBatch(impl_->tempTexture);
                }
            }
            break;
        }
        case DrawOp::Type::DrawImage: {
            impl_->flushColorBatch();

            const Image* image = recording.getImage(op.data.image.imageIndex);
            if (image && image->valid()) {
                GLuint texId = 0;
                if (image->isGpuBacked()) {
                    texId = static_cast<GLuint>(image->backendTextureHandle());
                } else if (gpuContext_) {
                    texId = static_cast<GLuint>(gpuContext_->resolveImageTexture(image));
                }
                if (texId == 0) break;

                float x = op.data.image.x;
                float y = op.data.image.y;
                float w = float(image->width());
                float h = float(image->height());
                impl_->pushTexQuad(x, y, x + w, y + h,
                                   0.0f, 0.0f, 1.0f, 1.0f);
                impl_->flushTexBatch(texId);
            }
            break;
        }
        case DrawOp::Type::SetClip: {
            impl_->flushColorBatch();
            Rect r = op.data.clip.rect;
            glEnable(GL_SCISSOR_TEST);
            // GL scissor is bottom-left origin, ink is top-left
            GLint sy = impl_->height - GLint(r.y + r.h);
            glScissor(GLint(r.x), sy, GLsizei(r.w), GLsizei(r.h));
            break;
        }
        case DrawOp::Type::ClearClip: {
            impl_->flushColorBatch();
            glDisable(GL_SCISSOR_TEST);
            break;
        }
        }
    }

    // Flush any remaining vertices
    impl_->flushColorBatch();
}

void GLBackend::readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const {
    GLuint targetFBO = impl_->useDefaultFBO ? 0 : impl_->fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dst);
}

unsigned int GLBackend::textureId() const {
    return impl_->colorTexture;
}

unsigned int GLBackend::fboId() const {
    return impl_->fbo;
}

} // namespace ink

#endif // INK_HAS_GL
