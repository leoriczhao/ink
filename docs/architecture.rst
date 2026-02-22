Architecture
============

Overview
--------

All drawing commands are first recorded, then sorted for optimal execution,
and finally dispatched to a backend for rendering.

::

   Canvas -> Device -> Recorder -> Recording -> DrawPass -> Backend

                                                        +-----+-----+
           |
                                                    CpuBackend  GLBackend
                                                    (software)  (OpenGL 3.3+)

                                                               GpuContext
                                                          (backend-agnostic)

                                                               GpuImpl
                                                          (internal virtual)

                                                             GLGpuImpl
                                                        (GL texture cache)

Core Components
---------------

- **Canvas** – User-facing drawing API with save/restore state stack and clip management
- **Device** – Converts Canvas commands into compact recorded operations
- **Recording** – Immutable command buffer with arena-allocated variable data
- **DrawPass** – Sorts operations by clip group, type, and color to minimize state changes
- **Backend** – Executes recorded operations on a specific rendering target
- **GpuContext** – Backend-agnostic shared GPU resource context, internally dispatches to backend-specific GpuImpl

Backends
--------

.. list-table::
   :header-rows: 1

   * - Backend
     - Status
     - Description
   * - CpuBackend
     - Working
     - Software rasterization to a Pixmap buffer
   * - GLBackend
     - Working
     - OpenGL 3.3+ rendering via FBO + GLSL shaders, vertex batching, scissor clipping, GPU snapshots
   * - VulkanBackend
     - Planned
     - Vulkan rendering

Surface Compositing
-------------------

Surfaces can be composited together using ``drawImage``. Snapshots are
backend-aware: CPU surfaces produce CPU-backed images, and GPU surfaces produce
GPU-backed images (texture handle, no readback required). When a CPU image is
drawn on a GPU surface, it is automatically uploaded and cached via ``GpuContext``.

.. code-block:: cpp

   // Render to offscreen surfaces
   auto layer1 = ink::Surface::MakeRaster(800, 600);
   layer1->beginFrame();
   layer1->canvas()->fillRect({0, 0, 800, 600}, {255, 0, 0, 128});
   layer1->endFrame();
   layer1->flush();

   // Composite onto final surface
   auto snapshot = layer1->makeSnapshot();
   finalSurface->canvas()->drawImage(snapshot, 0, 0);

Project Structure
-----------------

::

   ink/
   ├── CMakeLists.txt
   ├── include/ink/
   │   ├── ink.hpp              # Public umbrella header
   │   ├── version.hpp          # Version info
   │   ├── types.hpp            # Core types (Point, Rect, Color)
   │   ├── pixmap.hpp           # Pixel buffer management
   │   ├── pixel_data.hpp       # Non-owning pixel data descriptor
   │   ├── image.hpp            # Immutable pixel snapshot
   │   ├── canvas.hpp           # User-facing drawing API
   │   ├── device.hpp           # Recording device
   │   ├── recording.hpp        # Command recording and compact ops
   │   ├── draw_pass.hpp        # Command sorting
   │   ├── backend.hpp          # Abstract rendering backend
   │   ├── cpu_backend.hpp      # CPU software rasterizer
   │   ├── surface.hpp          # Top-level rendering target
   │   ├── glyph_cache.hpp      # Font rasterization
   │   └── gpu/
   │       ├── gpu_context.hpp
   │       └── gl/
   │           ├── gl_context.hpp
   │           └── gl_backend.hpp
   ├── src/
   │   ├── *.cpp
   │   └── gpu/
   │       └── gl/
   ├── tests/
   ├── examples/
   └── third_party/
       └── stb_truetype.h
