/**
 * example_basic.cpp - Basic drawing with ink (CPU + GPU)
 *
 * Demonstrates:
 *   - Creating both CPU and GPU surfaces
 *   - Drawing rectangles, lines, polylines on each
 *   - Using clip regions with save/restore
 *   - GPU readback via readPixels
 *   - Writing the result to a raw PPM file for viewing
 *
 * Build:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON -DINK_ENABLE_GL=ON && cmake --build build
 *   ./build/example_basic
 *
 * Output: basic_cpu.ppm, basic_gpu.ppm
 */

#include <ink/ink.hpp>
#include <fstream>
#include <cstdio>
#include <cstring>

#if INK_HAS_GL
#include <ink/gpu/gl_backend.hpp>
#include <ink/gpu/gpu_context.hpp>
#include <EGL/egl.h>
#endif

// Write BGRA pixmap to PPM
static void writePPM_BGRA(const char* filename, const void* data,
                           int w, int h, int stride) {
    std::ofstream f(filename, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    const auto* bytes = static_cast<const ink::u8*>(data);
    for (int y = 0; y < h; ++y) {
        const auto* row = reinterpret_cast<const ink::u32*>(bytes + y * stride);
        for (int x = 0; x < w; ++x) {
            ink::u32 p = row[x];
            ink::u8 b = p & 0xFF;
            ink::u8 g = (p >> 8) & 0xFF;
            ink::u8 r = (p >> 16) & 0xFF;
            f.put(char(r)); f.put(char(g)); f.put(char(b));
        }
    }
    std::printf("Written: %s (%dx%d)\n", filename, w, h);
}

// Write RGBA raw buffer to PPM (for GPU readback)
static void writePPM_RGBA(const char* filename, const void* data,
                           int w, int h) {
    std::ofstream f(filename, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    const auto* bytes = static_cast<const ink::u8*>(data);
    // GL readPixels returns bottom-up, flip vertically
    for (int y = h - 1; y >= 0; --y) {
        const auto* row = bytes + y * w * 4;
        for (int x = 0; x < w; ++x) {
            f.put(char(row[x * 4 + 0]));  // R
            f.put(char(row[x * 4 + 1]));  // G
            f.put(char(row[x * 4 + 2]));  // B
        }
    }
    std::printf("Written: %s (%dx%d)\n", filename, w, h);
}

// Draw the same scene on any surface
static void drawScene(ink::Canvas* canvas, int W, int H) {
    using ink::f32;

    // Background
    canvas->fillRect({0, 0, f32(W), f32(H)}, {40, 40, 50, 255});

    // Filled rectangles
    canvas->fillRect({20, 20, 160, 100}, {220, 60, 60, 255});
    canvas->fillRect({100, 60, 160, 100}, {60, 180, 60, 180});
    canvas->fillRect({200, 100, 160, 100}, {60, 60, 220, 255});

    // Stroked rectangle
    canvas->strokeRect({30, 180, 340, 80}, {255, 255, 0, 255});

    // Diagonal lines
    canvas->drawLine({0, 0}, {f32(W), f32(H)}, {255, 255, 255, 100}, 2.0f);
    canvas->drawLine({f32(W), 0}, {0, f32(H)}, {255, 255, 255, 100}, 2.0f);

    // Polyline triangle
    ink::Point tri[] = {{200, 30}, {260, 130}, {140, 130}, {200, 30}};
    canvas->drawPolyline(tri, 4, {255, 200, 0, 255}, 2.0f);

    // Clipped drawing
    canvas->save();
    canvas->clipRect({50, 200, 100, 50});
    canvas->fillRect({0, 0, f32(W), f32(H)}, {255, 0, 255, 200});
    canvas->restore();
}

int main() {
    const int W = 400, H = 300;

    // ---- CPU rendering ----
    {
        auto surface = ink::Surface::MakeRaster(W, H);
        surface->beginFrame();
        drawScene(surface->canvas(), W, H);
        surface->endFrame();
        surface->flush();

        auto* pm = surface->peekPixels();
        writePPM_BGRA("basic_cpu.ppm", pm->addr(), W, H, pm->info().stride);
    }

    // ---- GPU rendering ----
#if INK_HAS_GL
    {
        // Create headless EGL context
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) {
            std::printf("GPU: EGL display not available, skipping\n");
            return 0;
        }
        eglInitialize(display, nullptr, nullptr);

        EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        EGLConfig config;
        EGLint numConfigs;
        eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);

        EGLint pbufAttribs[] = {EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
        EGLSurface eglSurf = eglCreatePbufferSurface(display, config, pbufAttribs);

        eglBindAPI(EGL_OPENGL_API);
        EGLContext ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, nullptr);
        eglMakeCurrent(display, eglSurf, eglSurf, ctx);

        // Bind ink GPU context to the currently active host GL context
        auto gpuContext = ink::GpuContext::MakeGL();
        if (!gpuContext) {
            std::printf("GPU: failed to create ink::GpuContext\n");
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display, ctx);
            eglDestroySurface(display, eglSurf);
            eglTerminate(display);
            return 0;
        }

        // Create GPU surface with ink
        auto glBackend = ink::GLBackend::Make(gpuContext, W, H);
        if (!glBackend) {
            std::printf("GPU: failed to create GLBackend\n");
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display, ctx);
            eglDestroySurface(display, eglSurf);
            eglTerminate(display);
            return 0;
        }
        auto* glBackendPtr = static_cast<ink::GLBackend*>(glBackend.get());
        auto surface = ink::Surface::MakeGpu(std::move(glBackend), W, H);

        surface->beginFrame();
        drawScene(surface->canvas(), W, H);
        surface->endFrame();
        surface->flush();

        // Read pixels back from GPU
        std::vector<ink::u8> pixels(W * H * 4);
        glBackendPtr->readPixels(pixels.data(), 0, 0, W, H);

        writePPM_RGBA("basic_gpu.ppm", pixels.data(), W, H);

        std::printf("GPU: Rendered with OpenGL via GLBackend\n");

        // Cleanup EGL
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, ctx);
        eglDestroySurface(display, eglSurf);
        eglTerminate(display);
    }
#else
    std::printf("GPU: GL backend not available (build with -DINK_ENABLE_GL=ON)\n");
#endif

    return 0;
}
