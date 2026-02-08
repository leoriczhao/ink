/**
 * example_basic.cpp - Basic CPU drawing with ink
 *
 * Demonstrates:
 *   - Creating a raster surface
 *   - Drawing rectangles, lines, polylines
 *   - Using clip regions with save/restore
 *   - Writing the result to a raw PPM file for viewing
 *
 * Build:
 *   cmake -B build -DINK_BUILD_EXAMPLES=ON && cmake --build build
 *   ./build/example_basic
 *
 * Output: basic_output.ppm (open with any image viewer)
 */

#include <ink/ink.hpp>
#include <fstream>
#include <cstdio>

static void writePPM(const char* filename, const ink::Pixmap& pm) {
    std::ofstream f(filename, std::ios::binary);
    f << "P6\n" << pm.width() << " " << pm.height() << "\n255\n";

    for (ink::i32 y = 0; y < pm.height(); ++y) {
        const ink::u32* row = static_cast<const ink::u32*>(pm.rowAddr(y));
        for (ink::i32 x = 0; x < pm.width(); ++x) {
            ink::u32 pixel = row[x];
            // BGRA -> RGB
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
    const ink::i32 W = 400, H = 300;

    // 1. Create a CPU raster surface
    auto surface = ink::Surface::MakeRaster(W, H);

    // 2. Begin frame (clears to black)
    surface->beginFrame();
    auto* canvas = surface->canvas();

    // 3. Draw a filled background
    canvas->fillRect({0, 0, f32(W), f32(H)}, {40, 40, 50, 255});

    // 4. Draw some filled rectangles
    canvas->fillRect({20, 20, 160, 100}, {220, 60, 60, 255});   // red
    canvas->fillRect({100, 60, 160, 100}, {60, 180, 60, 180});  // green, semi-transparent
    canvas->fillRect({200, 100, 160, 100}, {60, 60, 220, 255}); // blue

    // 5. Draw stroked rectangle
    canvas->strokeRect({30, 180, 340, 80}, {255, 255, 0, 255});

    // 6. Draw lines
    canvas->drawLine({0, 0}, {f32(W), f32(H)}, {255, 255, 255, 100}, 1.0f);
    canvas->drawLine({f32(W), 0}, {0, f32(H)}, {255, 255, 255, 100}, 1.0f);

    // 7. Draw a polyline (triangle)
    ink::Point triangle[] = {
        {200, 30}, {260, 130}, {140, 130}, {200, 30}
    };
    canvas->drawPolyline(triangle, 4, {255, 200, 0, 255}, 1.0f);

    // 8. Clipped drawing: save state, set clip, draw, restore
    canvas->save();
    canvas->clipRect({50, 200, 100, 50});
    canvas->fillRect({0, 0, f32(W), f32(H)}, {255, 0, 255, 200}); // only visible in clip
    canvas->restore();

    // 9. End frame and flush (execute all recorded commands)
    surface->endFrame();
    surface->flush();

    // 10. Write output
    const ink::Pixmap* pm = surface->peekPixels();
    if (pm) {
        writePPM("basic_output.ppm", *pm);
    }

    return 0;
}
