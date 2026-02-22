#pragma once

/**
 * @file renderer.hpp
 * @brief Abstract rendering interface for CPU and GPU backends.
 */

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Recording;
class DrawPass;
class Image;
class GlyphCache;

/**
 * @brief Abstract rendering interface.
 *
 * Implemented by CPU and GPU renderers to execute drawing commands.
 * This abstraction allows Surface to work with any rendering backend
 * through a unified interface.
 */
class Renderer {
public:
    virtual ~Renderer() = default;

    /// @brief Begin a new frame, clearing the render target.
    /// @param clearColor The color to clear the render target to (default: opaque black).
    virtual void beginFrame(Color clearColor = {0, 0, 0, 255}) = 0;

    /// @brief End the current frame (no GPU submission; see flush()).
    virtual void endFrame() = 0;

    /// @brief Execute recorded drawing commands in the order defined by a draw pass.
    /// @param recording The recorded drawing commands.
    /// @param pass The draw pass defining execution order.
    virtual void execute(const Recording& recording, const DrawPass& pass) = 0;

    /// @brief Resize the render target.
    /// @param w New width in pixels.
    /// @param h New height in pixels.
    virtual void resize(i32 w, i32 h) = 0;

    /// @brief Create an immutable snapshot of the current render target contents.
    /// @return Shared pointer to the snapshot Image.
    virtual std::shared_ptr<Image> makeSnapshot() const = 0;

    /// @brief Set the glyph cache used for text rendering.
    /// @param cache Pointer to the glyph cache (may be null to disable text).
    virtual void setGlyphCache(GlyphCache* cache) { (void)cache; }
};

} // namespace ink
