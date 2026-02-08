/**
 * example_composite.cpp - Multi-layer compositing (CPU + GPU mixed)
 *
 * Demonstrates:
 *   - CPU surface for background grid
 *   - GPU surface for waveform rendering
 *   - GPU surface for UI overlay
 *   - Compositing all 3 layers via drawImage onto a final CPU surface
 *
 * Build:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON -DINK_ENABLE_GL=ON && cmake --build build
 *   ./build/example_composite
 *
 * Output: composite_output.ppm
 */

#include <ink/ink.hpp>
#include <fstream>
#include <cstdio>
#include <cmath>
#include <vector>

#if INK_HAS_GL
#include <ink/gpu/gl_backend.hpp>
#include <EGL/egl.h>
#endif

static void writePPM(const char* filename, const ink::Pixmap& pm) {
    std::ofstream f(filename, std::ios::binary);
    f << "P6\n" << pm.width() << " " << pm.height() << "\n255\n";
    for (ink::i32 y = 0; y < pm.height(); ++y) {
        const auto* row = static_cast<const ink::u32*>(pm.rowAddr(y));
        for (ink::i32 x = 0; x < pm.width(); ++x) {
            ink::u32 p = row[x];
            f.put(char((p >> 16) & 0xFF));  // R
            f.put(char((p >> 8) & 0xFF));   // G
            f.put(char(p & 0xFF));           // B
        }
    }
    std::printf("Written: %s (%dx%d)\n", filename, pm.width(), pm.height());
}

// Convert GPU readback (RGBA, bottom-up) to a CPU Pixmap (BGRA, top-down)
static void gpuReadbackToPixmap(ink::Pixmap& dst, const ink::u8* rgba, int w, int h) {
    for (int y = 0; y < h; ++y) {
        auto* dstRow = static_cast<ink::u32*>(dst.rowAddr(y));
        const auto* srcRow = rgba + (h - 1 - y) * w * 4;  // flip Y
        for (int x = 0; x < w; ++x) {
            ink::u8 r = srcRow[x * 4 + 0];
            ink::u8 g = srcRow[x * 4 + 1];
            ink::u8 b = srcRow[x * 4 + 2];
            ink::u8 a = srcRow[x * 4 + 3];
            // BGRA format
            dstRow[x] = (ink::u32(a) << 24) | (ink::u32(r) << 16) |
                        (ink::u32(g) << 8) | ink::u32(b);
        }
    }
}

int main() {
    using ink::f32;
    const ink::i32 W = 600, H = 400;

#if INK_HAS_GL
    // ---- Initialize EGL for headless GPU rendering ----
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    bool hasGPU = (display != EGL_NO_DISPLAY);
    EGLContext eglCtx = EGL_NO_CONTEXT;
    EGLSurface eglSurf = EGL_NO_SURFACE;

    if (hasGPU) {
        eglInitialize(display, nullptr, nullptr);
        EGLint cfgAttrs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        EGLConfig config;
        EGLint numCfg;
        eglChooseConfig(display, cfgAttrs, &config, 1, &numCfg);
        EGLint pbAttrs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
        eglSurf = eglCreatePbufferSurface(display, config, pbAttrs);
        eglBindAPI(EGL_OPENGL_API);
        eglCtx = eglCreateContext(display, config, EGL_NO_CONTEXT, nullptr);
        eglMakeCurrent(display, eglSurf, eglSurf, eglCtx);
        std::printf("GPU: EGL context created\n");
    } else {
        std::printf("GPU: EGL not available, using CPU for all layers\n");
    }
#else
    bool hasGPU = false;
    std::printf("GPU: GL backend not compiled, using CPU for all layers\n");
#endif

    // ---------------------------------------------------------------
    // Layer 1: Background grid (CPU rendered)
    // ---------------------------------------------------------------
    auto bgLayer = ink::Surface::MakeRaster(W, H);
    bgLayer->beginFrame();
    {
        auto* c = bgLayer->canvas();
        c->fillRect({0, 0, f32(W), f32(H)}, {25, 25, 35, 255});
        for (int x = 0; x < W; x += 40)
            c->drawLine({f32(x), 0}, {f32(x), f32(H)}, {50, 50, 60, 255});
        for (int y = 0; y < H; y += 40)
            c->drawLine({0, f32(y)}, {f32(W), f32(y)}, {50, 50, 60, 255});
        c->drawLine({0, f32(H/2)}, {f32(W), f32(H/2)}, {80, 80, 100, 255});
        c->drawLine({f32(W/2), 0}, {f32(W/2), f32(H)}, {80, 80, 100, 255});
    }
    bgLayer->endFrame();
    bgLayer->flush();
    std::printf("Layer 1 (background): CPU rendered\n");

    // ---------------------------------------------------------------
    // Layer 2: Waveform (GPU if available, else CPU)
    // ---------------------------------------------------------------
    std::unique_ptr<ink::Surface> waveLayer;
    ink::Pixmap wavePixmap;  // for GPU readback
#if INK_HAS_GL
    ink::GLBackend* waveBackendPtr = nullptr;
#endif

    if (hasGPU) {
#if INK_HAS_GL
        auto wb = ink::GLBackend::Make(W, H);
        waveBackendPtr = static_cast<ink::GLBackend*>(wb.get());
        waveLayer = ink::Surface::MakeGpu(std::move(wb), W, H);
#endif
    } else {
        waveLayer = ink::Surface::MakeRaster(W, H);
    }

    waveLayer->beginFrame();
    {
        auto* c = waveLayer->canvas();
        // For CPU surface, clear to transparent
        if (!hasGPU) {
            auto* pm = waveLayer->peekPixels();
            if (pm) pm->clear({0, 0, 0, 0});
        }

        const int N = W;
        std::vector<ink::Point> wave(N);
        for (int i = 0; i < N; ++i) {
            float t = f32(i) / f32(W) * 4.0f * 3.14159f;
            wave[i] = {f32(i), f32(H/2) + std::sin(t) * f32(H) * 0.3f};
        }
        c->drawPolyline(wave.data(), N, {0, 200, 255, 220}, 2.0f);

        for (int i = 0; i < N; ++i) {
            float t = f32(i) / f32(W) * 8.0f * 3.14159f;
            wave[i] = {f32(i), f32(H/2) + std::sin(t) * f32(H) * 0.1f};
        }
        c->drawPolyline(wave.data(), N, {255, 100, 50, 150}, 2.0f);
    }
    waveLayer->endFrame();
    waveLayer->flush();
    std::printf("Layer 2 (waveform):   %s rendered\n", hasGPU ? "GPU" : "CPU");

    // ---------------------------------------------------------------
    // Layer 3: UI overlay (GPU if available, else CPU)
    // ---------------------------------------------------------------
    std::unique_ptr<ink::Surface> uiLayer;
#if INK_HAS_GL
    ink::GLBackend* uiBackendPtr = nullptr;
#endif

    if (hasGPU) {
#if INK_HAS_GL
        auto ub = ink::GLBackend::Make(W, H);
        uiBackendPtr = static_cast<ink::GLBackend*>(ub.get());
        uiLayer = ink::Surface::MakeGpu(std::move(ub), W, H);
#endif
    } else {
        uiLayer = ink::Surface::MakeRaster(W, H);
    }

    uiLayer->beginFrame();
    {
        auto* c = uiLayer->canvas();
        if (!hasGPU) {
            auto* pm = uiLayer->peekPixels();
            if (pm) pm->clear({0, 0, 0, 0});
        }

        c->fillRect({10, 10, 180, 60}, {0, 0, 0, 160});
        c->strokeRect({10, 10, 180, 60}, {100, 100, 120, 200});

        f32 cx = f32(W/2), cy = f32(H/2);
        c->drawLine({cx - 15, cy}, {cx + 15, cy}, {255, 255, 0, 200}, 2.0f);
        c->drawLine({cx, cy - 15}, {cx, cy + 15}, {255, 255, 0, 200}, 2.0f);

        c->fillRect({0, 0, 8, 8}, {255, 0, 0, 255});
        c->fillRect({f32(W-8), 0, 8, 8}, {0, 255, 0, 255});
        c->fillRect({0, f32(H-8), 8, 8}, {0, 0, 255, 255});
        c->fillRect({f32(W-8), f32(H-8), 8, 8}, {255, 255, 0, 255});

        for (int x = 0; x < W; x += 100)
            c->fillRect({f32(x), f32(H-4), 2, 4}, {200, 200, 200, 180});
    }
    uiLayer->endFrame();
    uiLayer->flush();
    std::printf("Layer 3 (UI overlay): %s rendered\n", hasGPU ? "GPU" : "CPU");

    // ---------------------------------------------------------------
    // Create snapshots for compositing
    // For GPU surfaces, readback pixels and create Image from Pixmap
    // For CPU surfaces, just use makeSnapshot()
    // ---------------------------------------------------------------
    auto bgSnap = bgLayer->makeSnapshot();

    std::shared_ptr<ink::Image> waveSnap;
    std::shared_ptr<ink::Image> uiSnap;

    if (hasGPU) {
#if INK_HAS_GL
        // GPU -> CPU readback for waveform layer
        std::vector<ink::u8> buf(W * H * 4);
        auto tmpPm = ink::Pixmap::Alloc(ink::PixmapInfo::Make(W, H, ink::PixelFormat::BGRA8888));

        waveBackendPtr->readPixels(buf.data(), 0, 0, W, H);
        gpuReadbackToPixmap(tmpPm, buf.data(), W, H);
        waveSnap = ink::Image::MakeFromPixmap(tmpPm);

        uiBackendPtr->readPixels(buf.data(), 0, 0, W, H);
        gpuReadbackToPixmap(tmpPm, buf.data(), W, H);
        uiSnap = ink::Image::MakeFromPixmap(tmpPm);
#endif
    } else {
        waveSnap = waveLayer->makeSnapshot();
        uiSnap = uiLayer->makeSnapshot();
    }

    // ---------------------------------------------------------------
    // Composite all layers onto final CPU surface
    // ---------------------------------------------------------------
    auto finalSurface = ink::Surface::MakeRaster(W, H);
    finalSurface->beginFrame();
    {
        auto* c = finalSurface->canvas();
        c->drawImage(bgSnap, 0, 0);
        c->drawImage(waveSnap, 0, 0);
        c->drawImage(uiSnap, 0, 0);
    }
    finalSurface->endFrame();
    finalSurface->flush();

    auto* pm = finalSurface->peekPixels();
    if (pm) writePPM("composite_output.ppm", *pm);

    std::printf("\nCompositing: 3 layers -> drawImage -> final surface\n");

#if INK_HAS_GL
    if (hasGPU) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, eglCtx);
        eglDestroySurface(display, eglSurf);
        eglTerminate(display);
    }
#endif

    return 0;
}
