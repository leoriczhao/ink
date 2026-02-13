#pragma once

#include "ink/types.hpp"
#include "ink/pixmap.hpp"
#include "ink/recording.hpp"
#include "ink/draw_pass.hpp"
#include <memory>

namespace ink {

class GlyphCache;
class Image;

/**
 * Backend - Abstract base class for rendering backends.
 *
 * A Backend consumes a Recording (sorted by DrawPass) and executes the
 * draw operations. Different backends implement different rendering
 * strategies:
 *
 *   - CpuBackend: software rasterization to a Pixmap
 *   - GpuBackend (future): OpenGL / Vulkan / Metal rendering
 *
 * The Backend does NOT own the target pixel buffer or GPU resources.
 * Those are managed by the Surface that owns the Backend.
 */
class Backend {
public:
    virtual ~Backend() = default;

    /**
     * Execute all draw operations from a recording.
     * The DrawPass provides sorted indices for optimal execution order.
     */
    virtual void execute(const Recording& recording, const DrawPass& pass) = 0;

    /**
     * Called at the start of each frame to prepare the backend.
     * CPU: clears the target pixmap.
     * GPU: binds FBO, sets viewport, clears.
     */
    virtual void beginFrame() = 0;

    /**
     * Called at the end of each frame to finalize rendering.
     * CPU: no-op (pixels already written).
     * GPU: flush pending GL commands.
     */
    virtual void endFrame() = 0;

    /**
     * Resize the rendering target.
     */
    virtual void resize(i32 w, i32 h) = 0;

    /**
     * Set the glyph cache for text rendering.
     */
    virtual void setGlyphCache(GlyphCache* cache) { (void)cache; }

    /**
     * Create an immutable image snapshot of the backend render target.
     * CPU backends may return nullptr because Surface can snapshot directly
     * from Pixmap. GPU backends should return a GPU-backed Image when possible.
     */
    virtual std::shared_ptr<Image> makeSnapshot() const { return nullptr; }
};

} // namespace ink
