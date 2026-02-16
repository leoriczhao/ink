/**
 * example_basic.cpp - Basic drawing with ink, displayed via SDL2
 *
 * Build:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON && cmake --build build
 *   ./build/example_basic
 */

#include <ink/ink.hpp>
#include <cstdio>
#include <cmath>

#include <SDL2/SDL.h>

static void drawScene(ink::Canvas* canvas, int W, int H, float t) {
    using ink::f32;

    canvas->fillRect({0, 0, f32(W), f32(H)}, {40, 40, 50, 255});

    canvas->fillRect({20, 20, 160, 100}, {220, 60, 60, 255});
    canvas->fillRect({100, 60, 160, 100}, {60, 180, 60, 180});
    canvas->fillRect({200, 100, 160, 100}, {60, 60, 220, 255});

    canvas->strokeRect({30, 180, 340, 80}, {255, 255, 0, 255});

    canvas->drawLine({0, 0}, {f32(W), f32(H)}, {255, 255, 255, 100}, 2.0f);
    canvas->drawLine({f32(W), 0}, {0, f32(H)}, {255, 255, 255, 100}, 2.0f);

    f32 cx = f32(W) / 2 + std::sin(t) * 50;
    f32 cy = f32(H) / 2 + std::cos(t * 0.7f) * 30;
    ink::Point tri[] = {{cx, cy - 50}, {cx + 50, cy + 40}, {cx - 50, cy + 40}, {cx, cy - 50}};
    canvas->drawPolyline(tri, 4, {255, 200, 0, 255}, 2.0f);

    canvas->save();
    f32 clipX = 50 + std::sin(t * 0.5f) * 30;
    canvas->clipRect({clipX, 200, 100, 50});
    canvas->fillRect({0, 0, f32(W), f32(H)}, {255, 0, 255, 200});
    canvas->restore();
}

int main(int argc, char* argv[]) {
    const int W = 400, H = 300;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "ink - CPU Rendering",
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

    auto surface = ink::Surface::MakeRaster(W, H);

    std::printf("ink CPU rendering with SDL2 display\n");
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

        auto* pm = surface->peekPixels();
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
