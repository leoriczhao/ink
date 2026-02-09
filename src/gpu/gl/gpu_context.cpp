#include "ink/gpu/gpu_context.hpp"

#include "ink/image.hpp"

#if INK_HAS_GL
#include <GL/glew.h>
#include <unordered_map>
#include <cstdio>

namespace ink {

struct GpuContext::Impl {
    struct CachedCpuTexture {
        GLuint texture = 0;
        i32 width = 0;
        i32 height = 0;
        PixelFormat format = PixelFormat::BGRA8888;
    };

    std::unordered_map<u64, CachedCpuTexture> cpuImageCache;

    ~Impl() {
        for (auto& entry : cpuImageCache) {
            if (entry.second.texture != 0) {
                glDeleteTextures(1, &entry.second.texture);
            }
        }
        cpuImageCache.clear();
    }

    u32 cacheCpuImageTexture(const Image* image) {
        if (!image || !image->isCpuBacked() || !image->valid()) return 0;

        CachedCpuTexture cached;
        glGenTextures(1, &cached.texture);
        if (cached.texture == 0) return 0;

        glBindTexture(GL_TEXTURE_2D, cached.texture);
        const GLenum pixelFmt = (image->format() == PixelFormat::BGRA8888)
                                ? GL_BGRA
                                : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     image->width(), image->height(), 0,
                     pixelFmt, GL_UNSIGNED_BYTE, image->pixels());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        cached.width = image->width();
        cached.height = image->height();
        cached.format = image->format();

        cpuImageCache[image->uniqueId()] = cached;
        return cached.texture;
    }

    u32 resolveImageTexture(const Image* image) {
        if (!image || !image->valid()) return 0;
        if (image->isGpuBacked()) {
            return image->glTextureId();
        }

        const u64 id = image->uniqueId();
        auto it = cpuImageCache.find(id);
        if (it != cpuImageCache.end()) {
            return it->second.texture;
        }
        return cacheCpuImageTexture(image);
    }
};

GpuContext::GpuContext(std::shared_ptr<Impl> impl)
    : impl_(std::move(impl)) {
}

GpuContext::~GpuContext() = default;

std::shared_ptr<GpuContext> GpuContext::MakeGLFromCurrent() {
    // GLEW requires this for core profiles and EGL environments.
    glewExperimental = GL_TRUE;
    const GLenum err = glewInit();
    // Clear any sticky errors introduced by glewInit.
    while (glGetError() != GL_NO_ERROR) {
    }

    if (err != GLEW_OK) {
        // EGL setups can report non-fatal GLEW init errors; keep going if GL is alive.
        if (glGetString(GL_VERSION) == nullptr) {
            std::fprintf(stderr, "ink GpuContext: GLEW init failed: %s\n",
                         glewGetErrorString(err));
            return nullptr;
        }
    }

    if (glGetString(GL_VERSION) == nullptr) {
        std::fprintf(stderr, "ink GpuContext: no current GL context\n");
        return nullptr;
    }

    return std::shared_ptr<GpuContext>(new GpuContext(std::make_shared<Impl>()));
}

u32 GpuContext::resolveImageTexture(const Image* image) {
    if (!impl_) return 0;
    return impl_->resolveImageTexture(image);
}

} // namespace ink

#else

namespace ink {

struct GpuContext::Impl {
};

GpuContext::GpuContext(std::shared_ptr<Impl> impl)
    : impl_(std::move(impl)) {
}

GpuContext::~GpuContext() = default;

std::shared_ptr<GpuContext> GpuContext::MakeGLFromCurrent() {
    return nullptr;
}

u32 GpuContext::resolveImageTexture(const Image*) {
    return 0;
}

} // namespace ink

#endif
