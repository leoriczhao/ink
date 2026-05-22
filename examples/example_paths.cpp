/**
 * example_paths.cpp - Path drawing, path clipping, and dashed strokes with SDL2
 *
 * Build:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON && cmake --build build
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON -DINK_ENABLE_GL=ON
 *   cmake --build build
 *   ./build/example_paths
 */

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ink/ink.hpp>
#include <memory>
#include <vector>

#include <SDL.h>

#if INK_HAS_GL
#include <ink/gpu/gl/gl_context.hpp>
#endif

static ink::Path makeRibbon(float t) {
  ink::Path path;
  path.moveTo(60, 190)
      .cubicTo(140, 70 + std::sin(t) * 35, 260, 300, 340, 160)
      .cubicTo(410, 40, 500, 110 + std::cos(t * 0.8f) * 30, 560, 210);
  return path;
}

static ink::Path makeStar(ink::f32 cx, ink::f32 cy, ink::f32 outer,
                          ink::f32 inner, float rotation) {
  constexpr int kPoints = 10;
  constexpr ink::f32 kPi = 3.1415926535f;

  ink::Path star;
  for (int i = 0; i < kPoints; ++i) {
    ink::f32 radius = (i % 2 == 0) ? outer : inner;
    ink::f32 angle = rotation - kPi * 0.5f + ink::f32(i) * kPi / 5.0f;
    ink::Point p = {cx + std::cos(angle) * radius,
                    cy + std::sin(angle) * radius};
    if (i == 0) {
      star.moveTo(p);
    } else {
      star.lineTo(p);
    }
  }
  star.close();
  return star;
}

static void drawGrid(ink::Canvas *canvas, int width, int height) {
  using ink::f32;

  canvas->fillRect({0, 0, f32(width), f32(height)}, {24, 28, 34, 255});
  for (int x = 0; x <= width; x += 40) {
    canvas->drawLine({f32(x), 0}, {f32(x), f32(height)}, {44, 50, 60, 255});
  }
  for (int y = 0; y <= height; y += 40) {
    canvas->drawLine({0, f32(y)}, {f32(width), f32(y)}, {44, 50, 60, 255});
  }
}

static void drawScene(ink::Canvas *canvas, int width, int height, float t) {
  using ink::f32;

  drawGrid(canvas, width, height);

  ink::Path clip;
  clip.addRoundRect({35, 35, f32(width - 70), f32(height - 70)}, 36, 36);

  canvas->save();
  canvas->clipPath(clip);

  for (int i = 0; i < 9; ++i) {
    f32 x = 40.0f + f32(i) * 70.0f + std::sin(t + f32(i)) * 18.0f;
    f32 y = 80.0f + std::cos(t * 0.7f + f32(i) * 0.6f) * 24.0f;
    canvas->fillCircle(x, y, 44.0f,
                       {ink::u8(50 + i * 18), ink::u8(120 + i * 8), 210, 110});
  }

  ink::Path donut;
  donut.addCircle(f32(width) * 0.5f, f32(height) * 0.52f, 112.0f);
  donut.addCircle(f32(width) * 0.5f, f32(height) * 0.52f, 54.0f);
  donut.setFillRule(ink::FillRule::EvenOdd);

  ink::Paint fill = ink::Paint::Fill({54, 140, 230, 210});
  fill.antiAlias = true;
  canvas->drawPath(donut, fill);

  ink::Path star =
      makeStar(f32(width) * 0.5f, f32(height) * 0.52f, 46.0f, 19.0f, t);
  ink::Paint starFill = ink::Paint::Fill({255, 216, 92, 235});
  canvas->drawPath(star, starFill);

  canvas->restore();

  ink::Paint clipStroke = ink::Paint::Stroke({230, 235, 245, 230}, 3.0f);
  clipStroke.antiAlias = true;
  canvas->drawPath(clip, clipStroke);

  ink::Path ribbon = makeRibbon(t);
  ink::Paint ribbonStroke = ink::Paint::Stroke({255, 255, 255, 240}, 5.0f);
  ribbonStroke.antiAlias = true;
  ribbonStroke.cap = static_cast<ink::u8>(ink::LineCap::Round);
  ribbonStroke.join = static_cast<ink::u8>(ink::LineJoin::Round);
  f32 dash[] = {18.0f, 9.0f};
  ribbonStroke.pathEffect = ink::PathEffects::MakeDash(dash, 2, t * 14.0f);
  canvas->drawPath(ribbon, ribbonStroke);
}

#if INK_HAS_GL

static SDL_GLContext g_glContext = nullptr;

static std::shared_ptr<ink::GpuContext> createGpuContext(SDL_Window *window) {
  g_glContext = SDL_GL_CreateContext(window);
  if (!g_glContext) {
    std::printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    return nullptr;
  }

  auto ctx = ink::GpuContexts::MakeGL();
  if (!ctx) {
    std::printf("Failed to create GL GpuContext\n");
  }
  return ctx;
}

static void makeGpuBackendCurrent(SDL_Window *window) {
  if (g_glContext) {
    SDL_GL_MakeCurrent(window, g_glContext);
  }
}

static void destroyGpuBackend() {
  if (g_glContext) {
    SDL_GL_DeleteContext(g_glContext);
    g_glContext = nullptr;
  }
}

#endif

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  const int width = 640;
  const int height = 420;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::printf("SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

#if INK_HAS_GL
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

  Uint32 windowFlags = SDL_WINDOW_SHOWN;
#if INK_HAS_GL
  windowFlags |= SDL_WINDOW_OPENGL;
#endif

  SDL_Window *window =
      SDL_CreateWindow("ink - Paths and Curves", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, width, height, windowFlags);
  if (!window) {
    std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

#if INK_HAS_GL
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
#else
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#endif
  if (!renderer) {
    std::printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_Texture *texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                        SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!texture) {
    std::printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  std::shared_ptr<ink::GpuContext> gpuContext;
  std::unique_ptr<ink::Surface> surface;

#if INK_HAS_GL
  gpuContext = createGpuContext(window);
  if (gpuContext) {
    surface = ink::Surface::MakeGpu(gpuContext, width, height,
                                    ink::PixelFormat::RGBA8888);
  }
  if (!surface || !surface->isGPU()) {
    std::printf("GL path surface unavailable; falling back to CPU raster\n");
    surface =
        ink::Surface::MakeRaster(width, height, ink::PixelFormat::RGBA8888);
    gpuContext.reset();
    destroyGpuBackend();
  }
#else
  std::printf("example_paths was built without GL; rebuild with "
              "-DINK_ENABLE_GL=ON for OpenGL path rendering\n");
  surface = ink::Surface::MakeRaster(width, height, ink::PixelFormat::RGBA8888);
#endif

  if (!surface) {
    std::printf("Failed to create path example surface\n");
    gpuContext.reset();
#if INK_HAS_GL
    destroyGpuBackend();
#endif
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  std::vector<ink::u8> gpuPixels;
  std::vector<ink::u8> flippedPixels;
  if (surface->isGPU()) {
    gpuPixels.resize(width * height * 4);
    flippedPixels.resize(width * height * 4);
  }

  std::printf("ink paths, clipPath, and dash effects with SDL2 display\n");
  std::printf("Surface backend: %s\n", surface->isGPU() ? "OpenGL" : "CPU");
  std::printf("Press ESC or close window to exit\n");

  bool running = true;
  float t = 0.0f;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      }
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        running = false;
      }
    }

    t += 0.02f;

#if INK_HAS_GL
    if (surface->isGPU()) {
      makeGpuBackendCurrent(window);
    }
#endif

    surface->beginFrame();
    drawScene(surface->canvas(), width, height, t);
    surface->endFrame();
    surface->flush();

    if (surface->isGPU() && gpuContext) {
#if INK_HAS_GL
      makeGpuBackendCurrent(window);
#endif
      gpuContext->readPixels(gpuPixels.data(), 0, 0, width, height);
      for (int y = 0; y < height; ++y) {
        std::memcpy(flippedPixels.data() + y * width * 4,
                    gpuPixels.data() + (height - 1 - y) * width * 4, width * 4);
      }
      SDL_UpdateTexture(texture, nullptr, flippedPixels.data(), width * 4);
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderPresent(renderer);
    } else if (auto *pm = surface->peekPixels()) {
      SDL_UpdateTexture(texture, nullptr, pm->addr(), pm->info().stride);
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderPresent(renderer);
    }

    SDL_Delay(16);
  }

  surface.reset();
  gpuContext.reset();
#if INK_HAS_GL
  destroyGpuBackend();
#endif

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
