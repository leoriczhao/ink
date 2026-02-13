/**
 * example_composite.cpp - Multi-layer compositing with SDL2 display
 *
 * Build:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON && cmake --build build
 *   ./build/example_composite
 */

#include <ink/ink.hpp>
#include <cstdio>
#include <cmath>
#include <vector>

#include <SDL2/SDL.h>

static void drawBackground(ink::Canvas* c, int W, int H) {
    using ink::f32;
    c->fillRect({0, 0, f32(W), f32(H)}, {25, 25, 35, 255});
    for (int x = 0; x < W; x += 40)
        c->drawLine({f32(x), 0}, {f32(x), f32(H)}, {50, 50, 60, 255});
    for (int y = 0; y < H; y += 40)
        c->drawLine({0, f32(y)}, {f32(W), f32(y)}, {50, 50, 60, 255});
    c->drawLine({0, f32(H/2)}, {f32(W), f32(H/2)}, {80, 80, 100, 255});
    c->drawLine({f32(W/2), 0}, {f32(W/2), f32(H)}, {80, 80, 100, 255});
}

static void drawWaveform(ink::Canvas* c, int W, int H, float t) {
    using ink::f32;
    const int N = W;
    std::vector<ink::Point> wave(N);

    for (int i = 0; i < N; ++i) {
        float tt = f32(i) / f32(W) * 4.0f * 3.14159f + t;
        wave[i] = {f32(i), f32(H/2) + std::sin(tt) * f32(H) * 0.3f};
    }
    c->drawPolyline(wave.data(), N, {0, 200, 255, 220}, 2.0f);

    for (int i = 0; i < N; ++i) {
        float tt = f32(i) / f32(W) * 8.0f * 3.14159f + t * 1.5f;
        wave[i] = {f32(i), f32(H/2) + std::sin(tt) * f32(H) * 0.1f};
    }
    c->drawPolyline(wave.data(), N, {255, 100, 50, 150}, 2.0f);
}

static void drawUI(ink::Canvas* c, int W, int H, float t) {
    using ink::f32;

    c->fillRect({10, 10, 180, 60}, {0, 0, 0, 160});
    c->strokeRect({10, 10, 180, 60}, {100, 100, 120, 200});

    f32 cx = f32(W/2), cy = f32(H/2);
    c->drawLine({cx - 15, cy}, {cx + 15, cy}, {255, 255, 0, 200}, 2.0f);
    c->drawLine({cx, cy - 15}, {cx, cy + 15}, {255, 255, 0, 200}, 2.0f);

    c->fillRect({0, 0, 8, 8}, {255, 0, 0, 255});
    c->fillRect({f32(W-8), 0, 8, 8}, {0, 255, 0, 255});
    c->fillRect({0, f32(H-8), 8, 8}, {0, 0, 255, 255});
    c->fillRect({f32(W-8), f32(H-8), 8, 8}, {255, 255, 0, 255});

    f32 indicatorX = f32(W) * (0.5f + std::sin(t * 0.3f) * 0.4f);
    c->fillRect({indicatorX, f32(H-8), 4, 8}, {255, 255, 255, 255});
}

int main(int argc, char* argv[]) {
    using ink::f32;
    const ink::i32 W = 600, H = 400;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "ink - Multi-layer Compositing",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        W, H
    );

    auto bgLayer = ink::Surface::MakeRaster(W, H);
    auto waveLayer = ink::Surface::MakeRaster(W, H);
    auto uiLayer = ink::Surface::MakeRaster(W, H);
    auto finalSurface = ink::Surface::MakeRaster(W, H);

    bgLayer->beginFrame();
    drawBackground(bgLayer->canvas(), W, H);
    bgLayer->endFrame();
    bgLayer->flush();

    std::printf("ink multi-layer compositing with SDL2 display\n");
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

        auto* pm = waveLayer->peekPixels();
        if (pm) pm->clear({0, 0, 0, 0});
        waveLayer->beginFrame();
        drawWaveform(waveLayer->canvas(), W, H, t);
        waveLayer->endFrame();
        waveLayer->flush();

        pm = uiLayer->peekPixels();
        if (pm) pm->clear({0, 0, 0, 0});
        uiLayer->beginFrame();
        drawUI(uiLayer->canvas(), W, H, t);
        uiLayer->endFrame();
        uiLayer->flush();

        auto bgSnap = bgLayer->makeSnapshot();
        auto waveSnap = waveLayer->makeSnapshot();
        auto uiSnap = uiLayer->makeSnapshot();

        finalSurface->beginFrame();
        auto* c = finalSurface->canvas();
        c->drawImage(bgSnap, 0, 0);
        c->drawImage(waveSnap, 0, 0);
        c->drawImage(uiSnap, 0, 0);
        finalSurface->endFrame();
        finalSurface->flush();

        pm = finalSurface->peekPixels();
        if (pm) {
            SDL_UpdateTexture(texture, nullptr, pm->addr(), pm->info().stride);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }

        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
