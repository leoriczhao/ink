Backend Selection
=================

ink supports multiple rendering backends through a unified API. The drawing
code is identical regardless of which backend is active.

Available Backends
------------------

.. list-table::
   :header-rows: 1

   * - Backend
     - Status
     - Description
   * - **CPU (Software)**
     - Working
     - Software rasterization to a ``Pixmap`` buffer. No GPU required.
   * - **OpenGL 3.3+**
     - Working
     - Hardware-accelerated rendering via FBO, GLSL shaders, and vertex batching.
   * - **Metal**
     - Working
     - macOS / iOS hardware-accelerated rendering.
   * - **Vulkan**
     - Planned
     - Not yet implemented.

CPU Backend
-----------

The CPU backend is always available and requires no external dependencies:

.. code-block:: cpp

   auto surface = ink::Surface::MakeRaster(800, 600);

You can also wrap an existing pixel buffer (zero-copy):

.. code-block:: cpp

   auto info = ink::PixmapInfo::MakeBGRA(800, 600);
   auto surface = ink::Surface::MakeRasterDirect(info, myPixelBuffer);

OpenGL Backend
--------------

Enable the GL backend at build time with ``-DINK_ENABLE_GL=ON`` (default).

.. code-block:: cpp

   #include <ink/ink.hpp>
   #include <ink/gpu/gl/gl_context.hpp>

   // Caller must have a current GL context before calling MakeGL()
   auto ctx = ink::GpuContexts::MakeGL();
   auto surface = ink::Surface::MakeGpu(ctx, 800, 600);

.. important::

   The caller is responsible for ensuring the correct GL context is current
   before any ink GPU calls. If you use SDL with ``SDL_RENDERER_ACCELERATED``,
   it may create its own GL context that conflicts with ink's offscreen
   rendering. Use ``SDL_RENDERER_SOFTWARE`` for the SDL renderer when
   combining with ink's GL backend.

Metal Backend
-------------

On macOS, enable with ``-DINK_ENABLE_METAL=ON``:

.. code-block:: cpp

   #include <ink/gpu/metal/metal_context.hpp>

   auto ctx = ink::GpuContexts::MakeMetal();
   auto surface = ink::Surface::MakeGpu(ctx, 800, 600);

Metal does not share context state with SDL, so ``SDL_RENDERER_ACCELERATED``
works without issues.

Fallback Behavior
-----------------

``Surface::MakeGpu()`` falls back to a CPU surface if the provided
``GpuContext`` is null or invalid. This makes it safe to attempt GPU
creation without checking for errors:

.. code-block:: cpp

   auto ctx = ink::GpuContexts::MakeGL();  // may fail
   auto surface = ink::Surface::MakeGpu(ctx, 800, 600);
   // surface is valid either way (GPU or CPU fallback)
