#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Recording;
class DrawPass;
class Image;

/**
 * Renderer - Abstract rendering interface.
 *
 * Implemented by CPU and GPU renderers to execute drawing commands.
 * This abstraction allows Surface to work with any rendering backend
 * through a unified interface.
 */
class GlyphCache;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void beginFrame(Color clearColor = {0, 0, 0, 255}) = 0;
    virtual void endFrame() = 0;
    virtual void execute(const Recording& recording, const DrawPass& pass) = 0;
    virtual void resize(i32 w, i32 h) = 0;
    virtual std::shared_ptr<Image> makeSnapshot() const = 0;

    virtual void setGlyphCache(GlyphCache*) {}
};

} // namespace ink
