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
 *
 * Supports rendering to either:
 *   - An internal FBO (offscreen rendering, use readPixels to retrieve)
 *   - The default framebuffer (fbo=0, for direct-to-screen rendering)
 */
class GLBackend : public Backend {
public:
    /**
     * Create a GLBackend that renders to an internal FBO.
     * Host must have a current GL context.
     */
    static std::unique_ptr<Backend> Make(i32 w, i32 h);

    /**
     * Create a GLBackend that renders to the default framebuffer (fbo=0).
     * Useful when the host wants ink to draw directly to the window backbuffer.
     */
    static std::unique_ptr<Backend> MakeDefault(i32 w, i32 h);

    ~GLBackend() override;

    void execute(const Recording& recording, const DrawPass& pass) override;
    void beginFrame() override;
    void endFrame() override;
    void resize(i32 w, i32 h) override;
    void setGlyphCache(GlyphCache* cache) override;
    std::shared_ptr<Image> makeSnapshot() const override;

    /**
     * Read pixels from the FBO into a CPU buffer.
     * Format is RGBA8, top-left origin.
     * Buffer must be at least w * h * 4 bytes.
     */
    void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const;

    /**
     * Get the GL texture ID of the color attachment (for offscreen FBO mode).
     * Returns 0 if using default framebuffer.
     */
    unsigned int textureId() const;

    /**
     * Get the GL FBO ID. Returns 0 for default framebuffer mode.
     */
    unsigned int fboId() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    GLBackend(i32 w, i32 h, bool useDefaultFBO);
};

} // namespace ink

#endif // INK_HAS_GL
