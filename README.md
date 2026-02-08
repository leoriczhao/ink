# ink

A lightweight 2D rendering library with pluggable backends.

ink provides a simple Canvas API for drawing 2D primitives (rectangles, lines, polylines, text, images) with a unified record-then-execute architecture. All drawing commands are first recorded, then sorted for optimal execution, and finally dispatched to a backend for rendering.

## Architecture

```
Canvas -> Device -> Recorder -> Recording -> DrawPass -> Backend
                                                           |
                                                     +-----+-----+
                                                     |           |
                                                 CpuBackend  GLBackend
                                                 (software)  (OpenGL)
```

- **Canvas** - User-facing drawing API with save/restore state stack and clip management
- **Device** - Converts Canvas commands into compact recorded operations
- **Recording** - Immutable command buffer with arena-allocated variable data
- **DrawPass** - Sorts operations by clip group, type, and color to minimize state changes
- **Backend** - Executes recorded operations on a specific rendering target

### Backends

| Backend | Status | Description |
|---------|--------|-------------|
| CpuBackend | Working | Software rasterization to a Pixmap buffer |
| GLBackend | Stub | OpenGL rendering via FBO + shader pipelines |
| VulkanBackend | Planned | Vulkan rendering |

### Surface compositing

Surfaces can be composited together using `drawImage`:

```cpp
// Render to offscreen surfaces
auto layer1 = ink::Surface::MakeRaster(800, 600);
layer1->beginFrame();
layer1->canvas()->fillRect({0, 0, 800, 600}, {255, 0, 0, 128});
layer1->endFrame();
layer1->flush();

// Composite onto final surface
auto snapshot = layer1->makeSnapshot();
finalSurface->canvas()->drawImage(snapshot, 0, 0);
```

## Building

```bash
cmake -B build
cmake --build build
```

### Build options

| Option | Default | Description |
|--------|---------|-------------|
| `INK_ENABLE_GL` | `ON` | Enable OpenGL backend |
| `INK_ENABLE_VULKAN` | `OFF` | Enable Vulkan backend |
| `INK_BUILD_TESTS` | `OFF` | Build test suite |
| `INK_BUILD_EXAMPLES` | `OFF` | Build examples |
| `BUILD_SHARED_LIBS` | `OFF` | Build as shared library |

```bash
# CPU-only build (no GPU dependencies)
cmake -B build -DINK_ENABLE_GL=OFF

# With OpenGL
cmake -B build -DINK_ENABLE_GL=ON
```

## Integration

### As a CMake subdirectory

```cmake
add_subdirectory(third_party/ink)
target_link_libraries(my_app PRIVATE ink)
```

### Manual

Add `include/` to your include path and link against the built library. The only required dependency is a C++17 compiler. GPU backends require their respective graphics libraries.

## Usage

### Basic drawing

```cpp
#include <ink/ink.hpp>

// Create a CPU surface
auto surface = ink::Surface::MakeRaster(800, 600);

// Draw
surface->beginFrame();
auto* canvas = surface->canvas();
canvas->fillRect({10, 10, 200, 100}, {255, 0, 0, 255});
canvas->drawLine({0, 0}, {800, 600}, {255, 255, 255, 255}, 2.0f);
canvas->strokeRect({50, 50, 300, 200}, {0, 255, 0, 255});
surface->endFrame();
surface->flush();

// Access pixels
auto pd = surface->getPixelData();
// pd.data, pd.width, pd.height, pd.rowBytes are ready for display
```

### Text rendering

```cpp
ink::GlyphCache glyphs;
glyphs.init("/path/to/font.ttf", 16.0f);
surface->setGlyphCache(&glyphs);

surface->beginFrame();
surface->canvas()->drawText({10, 30}, "Hello, ink!", {255, 255, 255, 255});
surface->endFrame();
surface->flush();
```

### Multi-layer compositing

```cpp
// Create layers
auto background = ink::Surface::MakeRaster(800, 600);
auto overlay = ink::Surface::MakeRaster(800, 600);
auto final = ink::Surface::MakeRaster(800, 600);

// Render background
background->beginFrame();
background->canvas()->fillRect({0, 0, 800, 600}, {30, 30, 30, 255});
background->endFrame();
background->flush();

// Render overlay with transparency
overlay->beginFrame();
overlay->canvas()->fillRect({100, 100, 200, 200}, {255, 0, 0, 128});
overlay->endFrame();
overlay->flush();

// Composite
auto bgSnap = background->makeSnapshot();
auto ovSnap = overlay->makeSnapshot();

final->beginFrame();
final->canvas()->drawImage(bgSnap, 0, 0);
final->canvas()->drawImage(ovSnap, 0, 0);  // alpha-blended on top
final->endFrame();
final->flush();
```

## Project structure

```
ink/
├── CMakeLists.txt
├── include/ink/
│   ├── ink.hpp              # Public umbrella header
│   ├── types.hpp            # Core types (Point, Rect, Color)
│   ├── pixmap.hpp           # Pixel buffer management
│   ├── image.hpp            # Immutable pixel snapshot for compositing
│   ├── canvas.hpp           # User-facing drawing API
│   ├── device.hpp           # Recording device
│   ├── recording.hpp        # Command recording and compact ops
│   ├── draw_pass.hpp        # Command sorting for optimal execution
│   ├── backend.hpp          # Abstract rendering backend
│   ├── cpu_backend.hpp      # CPU software rasterizer
│   ├── pipeline.hpp         # GPU pipeline stage interface
│   ├── surface.hpp          # Top-level rendering target
│   ├── glyph_cache.hpp      # Font rasterization
│   └── gpu/
│       └── gl_backend.hpp   # OpenGL backend
├── src/
│   ├── *.cpp                # Core implementations
│   └── gpu/
│       └── gl/
│           └── gl_backend.cpp
└── third_party/
    └── stb_truetype.h       # Font rasterization
```

## License

MIT
