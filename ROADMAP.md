# ink Roadmap

Current version: **0.3.0**

> **Goal**: Build a production-grade, GPU-accelerated 2D rendering engine comparable to
> Skia in capability, performance, and platform coverage.

---

## Phase 1 — Core 2D Foundation (v0.3 – v0.5)

Establish the fundamental building blocks that every serious 2D engine requires: vector
paths, anti-aliasing, gradients, and a proper text stack.

### v0.3.0 — Paths and Curves

General-purpose vector path type — the heart of a 2D engine.

- **Path class**: moveTo, lineTo, quadTo, cubicTo, conicTo (weighted), close
- **Convenience builders**: addRect, addOval, addRoundRect, addArc, addCircle, addPoly
- **Path operations**: bounds, contains, direction (CW/CCW), isConvex detection
- **Fill rules**: Even-odd and winding via scanline rasterizer
- **Stroke engine**: Stroke-to-fill conversion with line joins (miter/round/bevel) and caps (butt/round/square)
- **Dash effects**: Dashed/dotted line styles via PathEffect abstraction
- Rewrite existing shape primitives (rect, circle, roundRect) as Path sugar
- **Canvas integration**: `drawPath(path, paint)`, path-based clip (`clipPath`)
- **GPU path rendering**: GL stencil-then-cover for filled/stroked paths and path clips; Metal path rendering remains a follow-up

### v0.4.0 — Anti-Aliasing and Gradients

Visual quality leap — coverage-based AA and rich fill styles.

- **Analytic anti-aliasing**: Coverage-based AA for path edges (CPU rasterizer)
- **GPU MSAA**: Multi-sample anti-aliasing support for GPU backends
- **Shader abstraction**: `Shader` base class pluggable into Paint
- **Linear gradient**: Two-point linear with arbitrary stops and tile modes (clamp/repeat/mirror)
- **Radial gradient**: Center + radius with stops and tile modes
- **Sweep (conic) gradient**: Angular gradient around a center point
- **Two-point conical gradient**: General conical gradient between two circles
- **Pattern (image) shader**: Tiled image fills with transform and tile modes
- GPU shader generation for all gradient types
- **ColorFilter abstraction**: Matrix color filter, blend-mode color filter, compose filter

### v0.5.0 — Text and Image Enhancements

Production-quality text pipeline and image handling.

- **Font abstraction**: `Typeface` (font file) and `Font` (typeface + size + options)
- **HarfBuzz integration**: Complex text shaping, ligatures, kerning
- **Unicode support**: ICU or equivalent for BiDi, line breaking, script detection
- **Font fallback**: Automatic fallback chain for missing glyphs
- **Sub-pixel text positioning**: Fractional glyph offsets for crisp text
- **Text blob**: Pre-shaped `TextBlob` for efficient repeated rendering
- **Image scaling filters**: Bilinear, bicubic, Mitchell-Netravali, Lanczos
- **drawImageRect**: Source/dest rect mapping with sub-region support
- **drawImageNine**: Nine-patch image drawing
- **Mipmaps**: Automatic mipmap generation for down-scaled images
- Glyph atlas overhaul: Multi-page atlas, SDF (signed distance field) glyphs for GPU

---

## Phase 2 — GPU Acceleration and Advanced Compositing (v0.6 – v0.8)

Transform ink from a basic renderer into a GPU-first engine with a modern compositing
model comparable to Skia's Ganesh/Graphite backends.

### v0.6.0 — Compositing Model and Filters

Full compositing stack and filter pipeline.

- **Save layer**: `saveLayer(bounds, paint)` for isolated compositing groups
- **Layer alpha and blend**: Per-layer opacity and blend mode
- **Backdrop filters**: Filters applied to content behind a layer
- **MaskFilter**: Blur (normal/solid/outer/inner), emboss
- **ImageFilter**: Drop shadow, blur, displacement map, morphology (dilate/erode)
- **ImageFilter DAG**: Compose and chain image filters
- **Color filters**: sRGB gamma, HSL adjust, color matrix, table lookup
- **PathEffect pipeline**: Dash, corner, discrete, compose, sum, trim
- Extend Porter-Duff to full **advanced blend modes**: Screen, Overlay, Darken, Lighten,
  ColorDodge, ColorBurn, HardLight, SoftLight, Difference, Exclusion, Multiply, Hue,
  Saturation, Color, Luminosity

### v0.7.0 — GPU Pipeline Overhaul

Modern GPU rendering architecture for high throughput.

- **Unified vertex pipeline**: Single GPU pipeline handling all geometry types
- **GPU transform propagation**: Canvas matrix properly passed to vertex/uniform stage
- **Instanced rendering**: Batch similar draws via GPU instancing
- **Texture atlas manager**: Shared atlas for glyphs, small images, gradient LUTs
- **Render pass optimization**: Minimize FBO/render-target switches
- **GPU path rasterizer**: Tessellation-based or compute-based path fill/stroke on GPU
- **Metal blend modes**: Full blend mode support on Metal (currently hardcoded SrcOver)
- **Vulkan backend**: Complete Vulkan 1.1+ implementation
  - Pipeline cache, descriptor set management, command buffer recording
  - Swapchain integration, synchronization
  - Memory allocator (VMA integration)
- **Render target management**: FBO/texture pool with automatic resizing and caching
- **GPU readback**: Async pixel readback for screenshots/testing

### v0.8.0 — Performance and Color

SIMD, threading, color management — the foundations of a professional renderer.

- **SIMD pixel ops**: SSE4.2 / AVX2 / NEON fast paths for blend, scale, convert
- **Tiled CPU rasterization**: Divide surface into tiles for cache efficiency
- **Parallel rasterization**: Thread-pool based parallel tile rendering
- **Color spaces**: sRGB, linear sRGB, Display P3, Adobe RGB, custom ICC
- **Color space conversion**: Automatic conversion in pipeline (source → working → output)
- **Wide color**: 16-bit half-float (F16) and 32-bit float (F32) pixel formats
- **HDR support**: PQ/HLG transfer functions, tone mapping
- **Premultiplied alpha**: Enforce correct premul throughout the pipeline
- **Dithering**: Ordered dithering for gradient banding reduction
- Comprehensive benchmark suite (CPU and GPU, per-primitive, per-backend)

---

## Phase 3 — Production Ready (v0.9 – v1.0)

Stability, API polish, platform completeness, and ecosystem readiness.

### v0.9.0 — Recording and Acceleration Structures

Efficient display lists and spatial optimization.

- **Picture / DisplayList**: Serializable recorded command buffer (like Skia `SkPicture`)
- **Picture playback**: Replay a recorded picture into any Canvas with transform
- **Picture serialization**: Binary format for caching / IPC / disk persistence
- **Bounding-box culling**: Skip draws outside the current clip during playback
- **Spatial index**: R-tree or grid for efficient hit testing and partial redraw
- **Dirty-rect tracking**: Incremental repaint — only redraw changed regions
- **GPU command deduplication**: Merge redundant state changes during DrawPass sort

### v1.0.0 — API Stability and Platform Polish

The 1.0 contract: stable API, full platform coverage, production quality.

- **API review and freeze**: Stable public API with semantic versioning guarantee
- **Thread safety model**: Document and enforce thread-safety contracts (Canvas not
  thread-safe, Typeface/Image shareable across threads, GpuContext single-threaded)
- **Error handling**: Consistent error reporting (no silent failures)
- **Memory management**: Arena allocators, pool allocators, configurable allocation hooks
- **Platform integration**:
  - Windows: Direct3D 11/12 backend or well-tested GL/Vulkan
  - macOS/iOS: Metal backend polished, CAMetalLayer integration
  - Linux: Vulkan + GL fallback, Wayland/X11 surface helpers
  - Android: OpenGL ES 3.0 / Vulkan, Surface/SurfaceTexture integration
  - WebAssembly: WebGL2 / WebGPU backend
- **Conformance test suite**: Golden-image tests across all backends
- **Fuzzing**: Fuzz the path parser, image decoder, recording deserialization
- **Documentation**: Complete API reference, architecture guide, integration cookbook

---

## Phase 4 — Advanced Capabilities (v1.x)

Features that put ink on par with Skia's full capability set.

### v1.1.0 — Compute-Based Rendering

Next-generation GPU path rendering using compute shaders.

- **GPU compute path rasterizer**: Implement a compute-shader-based path rendering
  pipeline (inspired by piet-gpu / Vello / Skia Graphite)
  - Path flattening on GPU
  - Tile-based binning
  - Per-tile coverage computation via compute shaders
  - Coarse + fine rasterization passes
- **Compute-based sorting**: GPU radix sort for draw order
- **Compute-based gradient evaluation**: Evaluate gradients in compute for complex stops
- **Sparse tiling**: Only process tiles that contain geometry

### v1.2.0 — Advanced Text

World-class text rendering matching platform-native quality.

- **Sub-pixel LCD rendering**: RGB sub-pixel text on LCD displays
- **Font variations**: Variable font (OpenType fvar) support
- **Color emoji**: COLR/CPAL, sbix, CBDT/CBLC color font tables
- **Text layout engine**: Paragraph layout, word wrap, alignment, indent, tab stops
- **Rich text**: Inline style runs (bold/italic/color spans)
- **Text decoration**: Underline, strikethrough, overline with proper skip-ink
- **Font rasterization backends**: Pluggable — stb_truetype (built-in), FreeType, CoreText, DirectWrite
- **Glyph path extraction**: Convert glyph outlines to ink::Path for effects

### v1.3.0 — Image Codecs and I/O

Built-in image decode/encode and document output.

- **Image codec framework**: Pluggable encoder/decoder registry
- **Built-in codecs**: PNG, JPEG, WebP, BMP, ICO, GIF (via stb_image or libpng/libjpeg-turbo)
- **Animated image**: Frame-based animated GIF/WebP/APNG playback
- **SVG importer**: Parse SVG subset into ink drawing commands
- **PDF backend**: Render to PDF via `Canvas` API (vector output, not rasterized)
- **SKP-like serialization**: Compact binary format for recorded pictures
- **GPU texture codecs**: ASTC, ETC2, BC (DXT) compressed texture support

### v1.4.0 — Advanced Geometry

Extended path operations and effects.

- **Path boolean operations**: Union, intersect, difference, XOR (via Clipper2 or custom)
- **Path simplification**: Remove self-intersections, reduce complexity
- **Path interpolation**: Morph between two compatible paths
- **Path measure**: Arc-length parameterization, getPosition/getTangent at distance
- **Conic sections**: Full conic (rational quadratic) support for perfect circles/ellipses
- **Stroke acceleration**: Offset curves for GPU stroke without fill conversion
- **Geometry shaders**: Custom vertex generation for effects (stipple, arrow heads)
- **Canvas drawVertices**: Arbitrary triangle mesh with per-vertex color and UV

---

## Phase 5 — Ecosystem and Platform (v2.x)

Build the ecosystem around the engine — bindings, tools, and cross-platform deployment.

### v2.0.0 — Architecture Evolution

Breaking changes for long-term scalability.

- **Graphite-style architecture**: Separate recording front-end from GPU back-end
  - Frontend: Platform/API agnostic command recording
  - Backend: Pluggable GPU translator (GL, Vulkan, Metal, D3D12, WebGPU)
- **Multi-threaded recording**: Record on multiple threads, submit to single GPU thread
- **Deferred GPU submission**: Build command buffers ahead of time, submit in batch
- **Resource management**: Explicit GPU resource lifetime, async upload/readback
- **Pipeline state objects**: Pre-compiled pipeline state for reduced draw-time overhead
- **Bindless textures**: Where supported, avoid texture bind overhead
- **Render graph**: Dependency-based render pass scheduling with automatic barriers

### v2.1.0 — Language Bindings and Integration

Make ink accessible from every major language and framework.

- **C API**: Stable C wrapper (`ink_c.h`) for FFI compatibility
- **Python bindings**: pybind11 or nanobind wrapper
- **Rust bindings**: Safe Rust wrapper crate (ink-rs)
- **JavaScript/TypeScript**: WebAssembly build + JS API wrapper
- **C# bindings**: P/Invoke wrapper for .NET / Unity
- **Java/Kotlin bindings**: JNI wrapper for Android / desktop Java
- **Flutter integration**: Custom `FlutterLayer` / external texture
- **Qt integration**: `QQuickItem` / `QWidget` backed by ink
- **Dear ImGui backend**: ink as an ImGui rendering backend

### v2.2.0 — Developer Tools

Tooling for debugging, profiling, and visualization.

- **Frame debugger**: Record and replay individual frames with command-level inspection
- **GPU profiler integration**: Timestamp queries, pipeline statistics
- **Overdraw visualization**: Color-coded overdraw heatmap
- **Batch visualization**: Visualize draw call batching and state changes
- **Resource inspector**: View textures, atlases, GPU memory usage
- **Trace output**: Chrome Tracing / Perfetto JSON trace format
- **CI visual regression**: Automated golden-image diff in CI pipeline

---

## Known Gaps vs Skia (Reference Checklist)

Tracking the key capabilities that Skia provides, for parity planning.

| Capability | Skia | ink (current) | Target Version |
|------------|------|---------------|----------------|
| Vector paths (cubic/conic/quad) | Yes | Yes | v0.3.0 |
| Analytic anti-aliasing | Yes | No | v0.4.0 |
| Gradient shaders (linear/radial/sweep) | Yes | No | v0.4.0 |
| HarfBuzz text shaping | Yes | No | v0.5.0 |
| Font fallback chain | Yes | No | v0.5.0 |
| Image filters (blur, shadow, etc.) | Yes | No | v0.6.0 |
| Save layer / compositing groups | Yes | No | v0.6.0 |
| Advanced blend modes (25+) | Yes | Porter-Duff only | v0.6.0 |
| Vulkan backend | Yes | Stub | v0.7.0 |
| SIMD-accelerated blitters | Yes | No | v0.8.0 |
| Color space management | Yes | No | v0.8.0 |
| SkPicture (display list) | Yes | No | v0.9.0 |
| D3D / WebGPU backends | Yes | No | v1.0.0 |
| GPU compute path rasterizer | Yes (Graphite) | No | v1.1.0 |
| Sub-pixel LCD text | Yes | No | v1.2.0 |
| Color emoji (COLR/sbix) | Yes | No | v1.2.0 |
| Text layout (paragraph) | Yes (via Skia Paragraph) | No | v1.2.0 |
| Image codecs (PNG/JPEG/WebP) | Yes | No | v1.3.0 |
| PDF output | Yes | No | v1.3.0 |
| SVG import/output | Yes | No | v1.3.0 |
| Path boolean operations | Yes (Pathops) | No | v1.4.0 |
| Multi-threaded recording | Yes (Graphite) | No | v2.0.0 |
| C / Python / language bindings | Partial | No | v2.1.0 |
| GPU frame debugger | Yes (Viewer) | No | v2.2.0 |

---

## Immediate Priorities (Next Steps)

Critical items to address after the v0.3.0 milestone:

1. **Implement Metal path rendering** — GL now has stencil-then-cover path rendering;
   Metal still needs equivalent fill/stroke/clipPath support.
2. **Fix Metal transform propagation** — GL applies Canvas matrix to vertex paths;
   Metal still needs equivalent transform propagation.
3. **Fix Metal blend modes** — Metal backend hardcodes SrcOver; should respect Paint's
   BlendMode like GL/CPU do.
4. **CPU ↔ GPU image interop** — CPU renderer silently drops GPU-only images; at minimum
   log a warning or implement readback.
5. **Increase test coverage** — GPU-path tests require a test harness that can run
   headless GL (e.g., EGL offscreen or Mesa llvmpipe in CI).

---

*This roadmap is a living document. Priorities may shift based on user demand, contributor
interest, and emerging GPU API trends. Contributions welcome at every level.*
