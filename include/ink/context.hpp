#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class GlyphCache;
class Recording;

class Context {
public:
    virtual ~Context() = default;

    // GPU context factories
    // Host must have already created and made current the GL context
    static std::unique_ptr<Context> MakeGL();

    virtual bool init(i32 w, i32 h) = 0;
    virtual void beginFrame() = 0;
    virtual void resize(i32 w, i32 h) = 0;
    virtual void submit(const Recording& recording) = 0;
    virtual void flush() = 0;
    virtual void setGlyphCache(GlyphCache* cache) = 0;
};

}
