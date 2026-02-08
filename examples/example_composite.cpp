/**
 * example_composite.cpp - Multi-layer compositing (CPU + GPU mixed)
 *
 * Demonstrates:
 *   - Creating multiple surfaces (simulating CPU + GPU layers)
 *   - Rendering different content on each layer
 *   - Compositing layers together using drawImage
 *   - The same pattern works when some layers are GPU-backed
 *
 * This example uses CPU surfaces only (GPU backend is a stub),
 * but the compositing API is identical for mixed CPU/GPU scenarios.
 *
 * Build:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON && cmake --build build
 *   ./build/example_composite
 *
 * Output: composite_output.ppm
 */

#include <ink/ink.hpp>
#include <fstream>
#include <cstdio>
#include <cmath>

static void writePPM(const char* filename, const ink::Pixmap& pm) {
    std::ofstream f(filename, std::ios::binary);
    f << "P6\n" << pm.width() << " " << pm.height() << "\n255\n";

    for (ink::i32 y = 0; y < pm.height(); ++y) {
        const ink::u32* row = static_cast<const ink::u32*>(pm.rowAddr(y));
        for (ink::i32 x = 0; x < pm.width(); ++x) {
            ink::u32 pixel = row[x];
            ink::u8 b = pixel & 0xFF;
            ink::u8 g = (pixel >> 8) & 0xFF;
            ink::u8 r = (pixel >> 16) & 0xFF;
            f.put(static_cast<char>(r));
            f.put(static_cast<char>(g));
            f.put(static_cast<char>(b));
        }
    }
    std::printf("Written: %s (%dx%d)\n", filename, pm.width(), pm.height());
}

int main() {
    using ink::f32;
    const ink::i32 W = 600, H = 400;

    // ---------------------------------------------------------------
    // Layer 1: Background (simulates a CPU-rendered layer)
    //   - Dark gradient-like background with grid lines
    // ---------------------------------------------------------------
    auto bgLayer = ink::Surface::MakeRaster(W, H);
    bgLayer->beginFrame();
    {
        auto* c = bgLayer->canvas();

        // Dark background
        c->fillRect({0, 0, f32(W), f32(H)}, {25, 25, 35, 255});

        // Grid lines
        for (int x = 0; x < W; x += 40) {
            c->drawLine({f32(x), 0}, {f32(x), f32(H)}, {50, 50, 60, 255});
        }
        for (int y = 0; y < H; y += 40) {
            c->drawLine({0, f32(y)}, {f32(W), f32(y)}, {50, 50, 60, 255});
        }

        // Axis lines
        c->drawLine({0, f32(H / 2)}, {f32(W), f32(H / 2)}, {80, 80, 100, 255});
        c->drawLine({f32(W / 2), 0}, {f32(W / 2), f32(H)}, {80, 80, 100, 255});
    }
    bgLayer->endFrame();
    bgLayer->flush();

    // ---------------------------------------------------------------
    // Layer 2: Waveform (simulates a GPU-rendered layer)
    //   - In a real app this would be Surface::MakeGpu(Backend::MakeGL(...))
    //   - Sine wave drawn as a polyline
    // ---------------------------------------------------------------
    auto waveLayer = ink::Surface::MakeRaster(W, H);
    waveLayer->beginFrame();
    {
        auto* c = waveLayer->canvas();
        // Transparent background (layer will be alpha-composited)
        // beginFrame already clears to black, we need transparent
        // For CPU surfaces, manually clear to transparent
        auto* pm = waveLayer->peekPixels();
        if (pm && pm->valid()) {
            pm->clear({0, 0, 0, 0});
        }

        // Generate sine wave points
        const int numPoints = W;
        std::vector<ink::Point> wave(numPoints);
        for (int i = 0; i < numPoints; ++i) {
            float t = f32(i) / f32(W) * 4.0f * 3.14159f;
            float amplitude = f32(H) * 0.3f;
            wave[i] = {f32(i), f32(H / 2) + std::sin(t) * amplitude};
        }

        // Draw the waveform
        c->drawPolyline(wave.data(), numPoints, {0, 200, 255, 220}, 1.0f);

        // Draw a second harmonic
        for (int i = 0; i < numPoints; ++i) {
            float t = f32(i) / f32(W) * 8.0f * 3.14159f;
            float amplitude = f32(H) * 0.1f;
            wave[i] = {f32(i), f32(H / 2) + std::sin(t) * amplitude};
        }
        c->drawPolyline(wave.data(), numPoints, {255, 100, 50, 150}, 1.0f);
    }
    waveLayer->endFrame();
    waveLayer->flush();

    // ---------------------------------------------------------------
    // Layer 3: UI overlay (simulates another GPU-rendered layer)
    //   - Labels, markers, info panel
    // ---------------------------------------------------------------
    auto uiLayer = ink::Surface::MakeRaster(W, H);
    uiLayer->beginFrame();
    {
        auto* c = uiLayer->canvas();
        auto* pm = uiLayer->peekPixels();
        if (pm && pm->valid()) {
            pm->clear({0, 0, 0, 0});
        }

        // Info panel background (semi-transparent)
        c->fillRect({10, 10, 180, 60}, {0, 0, 0, 160});
        c->strokeRect({10, 10, 180, 60}, {100, 100, 120, 200});

        // Marker crosshair at center
        f32 cx = f32(W / 2), cy = f32(H / 2);
        c->drawLine({cx - 15, cy}, {cx + 15, cy}, {255, 255, 0, 200});
        c->drawLine({cx, cy - 15}, {cx, cy + 15}, {255, 255, 0, 200});

        // Corner markers
        c->fillRect({0, 0, 8, 8}, {255, 0, 0, 255});
        c->fillRect({f32(W - 8), 0, 8, 8}, {0, 255, 0, 255});
        c->fillRect({0, f32(H - 8), 8, 8}, {0, 0, 255, 255});
        c->fillRect({f32(W - 8), f32(H - 8), 8, 8}, {255, 255, 0, 255});

        // Scale markers along bottom
        for (int x = 0; x < W; x += 100) {
            c->fillRect({f32(x), f32(H - 4), 2, 4}, {200, 200, 200, 180});
        }
    }
    uiLayer->endFrame();
    uiLayer->flush();

    // ---------------------------------------------------------------
    // Composite: combine all 3 layers onto the final surface
    //
    // This is the key part - in a real app:
    //   bgLayer    = CPU surface (software rendered grid)
    //   waveLayer  = GPU surface (hardware accelerated waveform)
    //   uiLayer    = GPU surface (hardware accelerated UI)
    //
    // The compositing code is identical regardless of backend:
    // ---------------------------------------------------------------
    auto finalSurface = ink::Surface::MakeRaster(W, H);

    // Take snapshots (immutable pixel copies)
    auto bgSnap   = bgLayer->makeSnapshot();
    auto waveSnap = waveLayer->makeSnapshot();
    auto uiSnap   = uiLayer->makeSnapshot();

    // Composite: bottom to top
    finalSurface->beginFrame();
    {
        auto* c = finalSurface->canvas();
        c->drawImage(bgSnap, 0, 0);     // Layer 1: background
        c->drawImage(waveSnap, 0, 0);   // Layer 2: waveform (alpha blended)
        c->drawImage(uiSnap, 0, 0);     // Layer 3: UI overlay (alpha blended)
    }
    finalSurface->endFrame();
    finalSurface->flush();

    // Write output
    const ink::Pixmap* pm = finalSurface->peekPixels();
    if (pm) {
        writePPM("composite_output.ppm", *pm);
    }

    std::printf("\nArchitecture demo:\n");
    std::printf("  Layer 1 (background): CPU surface -> makeSnapshot -> drawImage\n");
    std::printf("  Layer 2 (waveform):   CPU surface -> makeSnapshot -> drawImage\n");
    std::printf("  Layer 3 (UI overlay): CPU surface -> makeSnapshot -> drawImage\n");
    std::printf("  Final compositing:    3x drawImage with alpha blending\n");
    std::printf("\n");
    std::printf("In production, layers 2 & 3 would be GPU surfaces:\n");
    std::printf("  auto waveLayer = Surface::MakeGpu(Backend::MakeGL(W, H), W, H);\n");
    std::printf("  // ... same drawing code, same compositing code\n");

    return 0;
}
