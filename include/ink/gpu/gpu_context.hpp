#pragma once

/**
 * @file gpu_context.hpp
 * @brief GPU rendering context implementing the Renderer interface.
 */

#include "ink/renderer.hpp"
#include <memory>

namespace ink {

class GpuImpl;

/**
 * @brief GPU rendering context.
 *
 * Implements the Renderer interface for GPU rendering.
 * Create via backend-specific factory functions:
 *   - GpuContexts::MakeGL() (include ink/gpu/gl/gl_context.hpp)
 */
class GpuContext : public Renderer {
public:
    ~GpuContext() override;

    /// @brief Check if the underlying GPU implementation is valid.
    /// @return True if the GPU backend was initialized successfully.
    bool valid() const;

    /// @name Renderer interface
    /// @{

    /// @brief Begin a new frame, clearing the render target.
    /// @param clearColor The color to clear the render target to (default: opaque black).
    void beginFrame(Color clearColor = {0, 0, 0, 255}) override;

    /// @brief End the current frame.
    void endFrame() override;

    /// @brief Execute recorded drawing commands.
    /// @param recording The recorded drawing commands.
    /// @param pass The draw pass defining execution order.
    void execute(const Recording& recording, const DrawPass& pass) override;

    /// @brief Resize the render target.
    /// @param w New width in pixels.
    /// @param h New height in pixels.
    void resize(i32 w, i32 h) override;

    /// @brief Create an immutable snapshot of the current render target.
    /// @return Shared pointer to the snapshot Image.
    std::shared_ptr<Image> makeSnapshot() const override;

    /// @brief Set the glyph cache used for text rendering.
    /// @param cache Pointer to the glyph cache.
    void setGlyphCache(GlyphCache* cache) override;

    /// @}

    /// @name GPU-specific operations
    /// @{

    /// @brief Read pixels from the GPU framebuffer into a buffer.
    /// @param dst Destination buffer (must be large enough for w×h×4 bytes).
    /// @param x X offset in the framebuffer.
    /// @param y Y offset in the framebuffer.
    /// @param w Width of the region to read.
    /// @param h Height of the region to read.
    void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const;

    /// @brief Get the GL texture ID of the offscreen color attachment.
    /// @return OpenGL texture name.
    unsigned int textureId() const;

    /// @brief Get the GL framebuffer object ID.
    /// @return OpenGL FBO name.
    unsigned int fboId() const;

    /// @}

private:
    std::shared_ptr<GpuImpl> impl_;

    explicit GpuContext(std::shared_ptr<GpuImpl> impl);

    u64 resolveImageTexture(const Image* image);

    friend class Surface;
    friend std::shared_ptr<GpuContext> MakeGpuContextFromImpl(std::shared_ptr<GpuImpl>);
};

} // namespace ink
