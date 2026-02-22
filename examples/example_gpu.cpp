/**
 * example_gpu.cpp - GPU rendering with ink, displayed via SDL2
 *
 * Demonstrates:
 *   - Creating GpuContext via backend-specific factory
 *   - Creating GPU Surface via Surface::MakeGpu(ctx, w, h)
 *   - Real-time GPU rendering with pixel readback
 *
 * On macOS with Metal:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON -DINK_ENABLE_METAL=ON && cmake --build build
 *
 * On Linux with OpenGL:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON -DINK_ENABLE_GL=ON && cmake --build build
 *
 *   ./build/example_gpu
 */

#include <ink/ink.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>

#include <SDL.h>

// Backend-specific includes
#if INK_HAS_METAL
#include <ink/gpu/metal/metal_context.hpp>
#elif INK_HAS_GL
#include <ink/gpu/gl/gl_context.hpp>
#endif

static void drawScene(ink::Canvas* canvas, int W, int H, float t) {
    using ink::f32;

    canvas->fillRect({0, 0, f32(W), f32(H)}, {20, 25, 35, 255});

    for (int x = 0; x < W; x += 40)
        canvas->drawLine({f32(x), 0}, {f32(x), f32(H)}, {40, 45, 55, 255});
    for (int y = 0; y < H; y += 40)
        canvas->drawLine({0, f32(y)}, {f32(W), f32(y)}, {40, 45, 55, 255});

    f32 cx = f32(W) / 2;
    f32 cy = f32(H) / 2;
    for (int i = 0; i < 6; ++i) {
        float angle = t + i * 3.14159f / 3.0f;
        float r = 80 + std::sin(t * 2 + i) * 20;
        float x = cx + std::cos(angle) * r;
        float y = cy + std::sin(angle) * r;
        float size = 30 + std::sin(t * 3 + i * 0.5f) * 10;

        ink::u8 alpha = 150 + i * 17;
        canvas->fillRect({x - size/2, y - size/2, size, size},
                         {ink::u8(100 + i * 25), ink::u8(150 - i * 20), ink::u8(200 - i * 15), alpha});
    }

    std::vector<ink::Point> wave(W);
    for (int i = 0; i < W; ++i) {
        float tt = f32(i) / f32(W) * 4.0f * 3.14159f + t;
        wave[i] = {f32(i), cy + std::sin(tt) * 60};
    }
    canvas->drawPolyline(wave.data(), W, {0, 200, 255, 220}, 2.0f);

    for (int i = 0; i < W; ++i) {
        float tt = f32(i) / f32(W) * 6.0f * 3.14159f + t * 1.5f;
        wave[i] = {f32(i), cy + std::sin(tt) * 30};
    }
    canvas->drawPolyline(wave.data(), W, {255, 100, 50, 180}, 2.0f);

    canvas->fillRect({10, 10, 200, 40}, {0, 0, 0, 180});
    canvas->strokeRect({10, 10, 200, 40}, {100, 200, 255, 200});

    f32 fpsX = f32(W) - 100 + std::sin(t) * 20;
    canvas->fillRect({fpsX, f32(H) - 30, 80, 20}, {255, 200, 0, 200});
}

// ---- Backend initialization helpers ----

#if INK_HAS_METAL

static std::shared_ptr<ink::GpuContext> createGpuContext() {
    auto ctx = ink::GpuContexts::MakeMetal();
    if (!ctx) {
        std::printf("Failed to create Metal GpuContext\n");
    }
    return ctx;
}

static void destroyGpuBackend() {
    // Metal needs no explicit teardown
}

static const char* backendName() { return "Metal"; }

#elif INK_HAS_GL

static SDL_GLContext g_glContext = nullptr;

static std::shared_ptr<ink::GpuContext> createGpuContext(SDL_Window* window) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    g_glContext = SDL_GL_CreateContext(window);
    if (!g_glContext) {
        std::printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return nullptr;
    }

    auto gpuCtx = ink::GpuContexts::MakeGL();
    if (!gpuCtx) {
        std::printf("Failed to create GL GpuContext\n");
    }
    return gpuCtx;
}

static void destroyGpuBackend() {
    if (g_glContext) {
        SDL_GL_DeleteContext(g_glContext);
        g_glContext = nullptr;
    }
}

static const char* backendName() { return "OpenGL"; }

#else
#error "No GPU backend available. Build with -DINK_ENABLE_METAL=ON or -DINK_ENABLE_GL=ON"
#endif

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    const int W = 600, H = 400;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    char title[128];
    std::snprintf(title, sizeof(title), "ink - GPU Rendering (%s)", backendName());

    Uint32 windowFlags = SDL_WINDOW_SHOWN;
#if !INK_HAS_METAL && INK_HAS_GL
    windowFlags |= SDL_WINDOW_OPENGL;
#endif

    SDL_Window* window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H,
        windowFlags
    );
    if (!window) {
        std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

#if !INK_HAS_METAL && INK_HAS_GL
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
#else
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#endif
    if (!renderer) {
        std::printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        W, H
    );

    // ---- Create GPU context (backend-specific) ----
#if INK_HAS_METAL
    auto gpuContext = createGpuContext();
#elif INK_HAS_GL
    auto gpuContext = createGpuContext(window);
#endif
    if (!gpuContext) {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        destroyGpuBackend();
        return 1;
    }

    // ---- Create GPU Surface ----
    auto surface = ink::Surface::MakeGpu(gpuContext, W, H);
    if (!surface) {
        std::printf("Failed to create GPU Surface\n");
        gpuContext.reset();
        destroyGpuBackend();
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::vector<ink::u8> pixels(W * H * 4);

    std::printf("ink GPU rendering with %s + SDL2 display\n", backendName());
    std::printf("Surface is GPU: %s\n", surface->isGPU() ? "yes" : "no");
    std::printf("Press ESC or close window to exit\n");

    bool running = true;
    float t = 0.0f;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        t += 0.02f;

        surface->beginFrame();
        drawScene(surface->canvas(), W, H, t);
        surface->endFrame();
        surface->flush();

        // Read pixels from GPU via GpuContext::readPixels (RGBA format)
        gpuContext->readPixels(pixels.data(), 0, 0, W, H);

        // GPU framebuffers are typically bottom-up; flip vertically for SDL
        std::vector<ink::u8> flipped(W * H * 4);
        for (int y = 0; y < H; ++y) {
            std::memcpy(flipped.data() + y * W * 4,
                        pixels.data() + (H - 1 - y) * W * 4,
                        W * 4);
        }

        SDL_UpdateTexture(texture, nullptr, flipped.data(), W * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    // Cleanup
    surface.reset();
    gpuContext.reset();
    destroyGpuBackend();

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
