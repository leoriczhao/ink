#pragma once

// GL resource management utilities.
// Internal implementation - not part of public API.

#if INK_HAS_GL

#include "ink/types.hpp"
#include <GL/glew.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace ink {

class Image;

// Shader program management
class GLShaderProgram {
public:
    GLuint program = 0;
    GLint projLoc = -1;
    GLint samplerLoc = -1;

    bool init(const char* vertSrc, const char* fragSrc) {
        GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }
        program = linkProgram(vert, frag);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return program != 0;
    }

    void destroy() {
        if (program) { glDeleteProgram(program); program = 0; }
    }

    void use() const { glUseProgram(program); }

    void setProjection(float w, float h) const {
        float m[16];
        std::memset(m, 0, sizeof(m));
        m[0] = 2.0f / w; m[5] = -2.0f / h; m[10] = -1.0f;
        m[12] = -1.0f; m[13] = 1.0f; m[15] = 1.0f;
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, m);
    }

private:
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
};

// Vertex buffer with VAO
template<typename Vertex>
class GLVertexBuffer {
public:
    GLuint vao = 0;
    GLuint vbo = 0;

    void init() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }

    void destroy() {
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
        if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    }

    void upload(const std::vector<Vertex>& verts) const {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * sizeof(Vertex)),
                     verts.data(), GL_DYNAMIC_DRAW);
    }

    void bind() const { glBindVertexArray(vao); }

    void draw() const { glDrawArrays(GL_TRIANGLES, 0, boundCount); }

    GLsizei boundCount = 0;
};

// Texture cache for CPU images
class GLTextureCache {
public:
    ~GLTextureCache() { clear(); }

    GLuint resolve(const class Image* image);

    void clear() {
        for (auto& e : cache_) {
            if (e.second) glDeleteTextures(1, &e.second);
        }
        cache_.clear();
    }

private:
    std::unordered_map<u64, GLuint> cache_;
};

// Framebuffer with color texture
class GLFramebuffer {
public:
    GLuint fbo = 0;
    GLuint texture = 0;
    i32 width = 0;
    i32 height = 0;

    bool init(i32 w, i32 h) {
        width = w;
        height = h;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void destroy() {
        if (texture) { glDeleteTextures(1, &texture); texture = 0; }
        if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    }

    void resize(i32 w, i32 h) {
        width = w;
        height = h;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    void bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, width, height);
    }
};

// Color vertex (position + color)
struct GLColorVertex { float x, y, r, g, b, a; };

// Texture vertex (position + uv)
struct GLTexVertex { float x, y, u, v; };

} // namespace ink

#endif // INK_HAS_GL
