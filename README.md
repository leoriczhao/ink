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
                                                 (software)  (OpenGL 3.3+)
                                                                |
                                                            GpuContext
                                                       (backend-agnostic)
                                                                |
                                                            GpuImpl
                                                       (internal virtual)
                                                                |
                                                          GLGpuImpl
                                                     (GL texture cache)
```

- **Canvas** - User-facing drawing API with save/restore state stack and clip management
- **Device** - Converts Canvas commands into compact recorded operations
- **Recording** - Immutable command buffer with arena-allocated variable data
- **DrawPass** - Sorts operations by clip group, type, and color to minimize state changes
- **Backend** - Executes recorded operations on a specific rendering target
- **GpuContext** - Backend-agnostic shared GPU resource context, internally dispatches to backend-specific GpuImpl

### Backends

| Backend | Status | Description |
|---------|--------|-------------|
| CpuBackend | Working | Software rasterization to a Pixmap buffer |
| GLBackend | Working | OpenGL 3.3+ rendering via FBO + GLSL shaders, vertex batching, scissor clipping, GPU snapshots |
| VulkanBackend | Planned | Vulkan rendering |

### Surface compositing

Surfaces can be composited together using `drawImage`. Snapshots are backend-aware: CPU surfaces produce CPU-backed images, and GPU surfaces produce GPU-backed images (texture handle, no readback required). When a CPU image is drawn on a GPU surface, it is automatically uploaded and cached via `GpuContext`.

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

# Build with tests
cmake -B build -DINK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build

# Build with examples
cmake -B build -DINK_BUILD_EXAMPLES=ON
cmake --build build
```

## Integration

### As a CMake subdirectory

```cmake
add_subdirectory(third_party/ink)
target_link_libraries(my_app PRIVATE ink)
```

### Manual

Add `include/` to your include path and link against the built library.

### Requirements

- C++17 compiler (GCC, Clang, or MSVC)
- CMake 3.16+
- **Optional:** OpenGL + GLEW (for GL backend)
- **Test-only:** Google Test (fetched automatically via CMake FetchContent)

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

// Access pixels via PixelData (non-owning descriptor)
auto pd = surface->getPixelData();
// pd.data, pd.width, pd.height, pd.rowBytes, pd.format are ready for display
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

### GPU rendering

```cpp
#include <ink/ink.hpp>

// Auto-select: tries GPU first, falls back to CPU
auto surface = ink::Surface::MakeAuto(800, 600);

// Or explicitly create a GPU surface with a shared context
#include <ink/gpu/gl/gl_context.hpp>
auto ctx = ink::GpuContexts::MakeGL();
auto surface = ink::Surface::MakeGpu(ctx, 800, 600);

// Drawing API is identical regardless of backend
surface->beginFrame();
auto* canvas = surface->canvas();
canvas->fillRect({10, 10, 200, 100}, {255, 0, 0, 255});
canvas->drawLine({0, 0}, {800, 600}, {255, 255, 255, 255}, 2.0f);
surface->endFrame();
surface->flush();

// GPU snapshots produce GPU-backed images (no readback)
auto snapshot = surface->makeSnapshot();
```

### Multi-layer compositing

```cpp
// Create layers
auto background = ink::Surface::MakeRaster(800, 600);
auto overlay = ink::Surface::MakeRaster(800, 600);
auto composite = ink::Surface::MakeRaster(800, 600);

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

composite->beginFrame();
composite->canvas()->drawImage(bgSnap, 0, 0);
composite->canvas()->drawImage(ovSnap, 0, 0);  // alpha-blended on top
composite->endFrame();
composite->flush();
```

## Project structure

```
ink/
├── CMakeLists.txt
├── include/ink/
│   ├── ink.hpp              # Public umbrella header
│   ├── version.hpp          # Version info (major/minor/patch)
│   ├── types.hpp            # Core types (Point, Rect, Color)
│   ├── pixmap.hpp           # Pixel buffer management
│   ├── pixel_data.hpp       # Non-owning pixel data descriptor
│   ├── image.hpp            # Immutable pixel snapshot (CPU or GPU-backed)
│   ├── canvas.hpp           # User-facing drawing API
│   ├── device.hpp           # Recording device
│   ├── recording.hpp        # Command recording and compact ops
│   ├── draw_op_visitor.hpp  # Visitor interface for draw operations
│   ├── draw_pass.hpp        # Command sorting for optimal execution
│   ├── backend.hpp          # Abstract rendering backend
│   ├── cpu_backend.hpp      # CPU software rasterizer
│   ├── pipeline.hpp         # GPU pipeline stage interface
│   ├── surface.hpp          # Top-level rendering target
│   ├── glyph_cache.hpp      # Font rasterization
│   └── gpu/
│       ├── gpu_context.hpp  # Backend-agnostic shared GPU resource context
│       └── gl/
│           ├── gl_context.hpp   # GpuContexts::MakeGL() factory
│           └── gl_backend.hpp   # OpenGL 3.3+ backend
├── src/
│   ├── *.cpp                # Core implementations
│   └── gpu/
│       ├── gpu_context.cpp  # GpuContext shell (delegates to GpuImpl)
│       ├── gpu_impl.hpp     # Internal abstract base for GPU backends
│       └── gl/
│           ├── gl_context.cpp   # MakeGL() implementation
│           ├── gl_backend.cpp
│           └── gl_gpu_impl.cpp
├── tests/
│   └── *.cpp                # Google Test suite (9 test files)
├── examples/
│   ├── example_basic.cpp    # CPU + GPU drawing demo
│   └── example_composite.cpp # Multi-layer compositing demo
└── third_party/
    └── stb_truetype.h       # Font rasterization
```

## License

MIT
