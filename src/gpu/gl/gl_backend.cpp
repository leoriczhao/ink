// GLBackend - OpenGL rendering backend (stub)
//
// This file is only compiled when INK_HAS_GL is defined.
// TODO: Implement OpenGL rendering pipelines.

#include "ink/gpu/gl_backend.hpp"

#if INK_HAS_GL

namespace ink {

struct GLBackend::Impl {
    i32 width = 0;
    i32 height = 0;
    GlyphCache* glyphCache = nullptr;
    // TODO: GLuint fbo, colorTexture
    // TODO: RectPipeline, LinePipeline, TextPipeline, ImagePipeline
};

GLBackend::GLBackend(i32 w, i32 h)
    : impl_(std::make_unique<Impl>()) {
    impl_->width = w;
    impl_->height = h;
}

GLBackend::~GLBackend() = default;

std::unique_ptr<Backend> GLBackend::Make(i32 w, i32 h) {
    return std::unique_ptr<Backend>(new GLBackend(w, h));
}

void GLBackend::execute(const Recording& /*recording*/, const DrawPass& /*pass*/) {
    // TODO: iterate sorted ops, dispatch to GL pipelines
}

void GLBackend::beginFrame() {
    // TODO: glBindFramebuffer, glViewport, glClear
}

void GLBackend::endFrame() {
    // TODO: glFlush
}

void GLBackend::resize(i32 w, i32 h) {
    impl_->width = w;
    impl_->height = h;
    // TODO: recreate FBO / texture
}

void GLBackend::setGlyphCache(GlyphCache* cache) {
    impl_->glyphCache = cache;
}

} // namespace ink

#endif // INK_HAS_GL
