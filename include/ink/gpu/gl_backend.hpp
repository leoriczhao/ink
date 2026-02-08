#pragma once

#include "ink/backend.hpp"

#if INK_HAS_GL

namespace ink {

/**
 * GLBackend - OpenGL rendering backend.
 *
 * Renders to an FBO (framebuffer object) using OpenGL.
 * Host must have created and made current an OpenGL context before
 * creating this backend.
 */
class GLBackend : public Backend {
public:
    static std::unique_ptr<Backend> Make(i32 w, i32 h);

    ~GLBackend() override;

    void execute(const Recording& recording, const DrawPass& pass) override;
    void beginFrame() override;
    void endFrame() override;
    void resize(i32 w, i32 h) override;
    void setGlyphCache(GlyphCache* cache) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    GLBackend(i32 w, i32 h);
};

} // namespace ink

#endif // INK_HAS_GL
