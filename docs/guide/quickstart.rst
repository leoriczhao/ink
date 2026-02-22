Quick Start
===========

Installation
------------

.. code-block:: bash

   git clone https://github.com/leoriczhao/ink.git
   cd ink
   cmake -B build -DINK_BUILD_TESTS=ON
   cmake --build build -j$(nproc)

Hello ink
---------

The simplest ink program creates a CPU surface, draws a red rectangle, and
accesses the resulting pixels:

.. code-block:: cpp

   #include <ink/ink.hpp>

   int main() {
       // Create a CPU-backed surface (800×600, BGRA)
       auto surface = ink::Surface::MakeRaster(800, 600);

       // Begin a frame — clears to opaque black by default
       surface->beginFrame();

       // Draw through the Canvas API
       auto* canvas = surface->canvas();
       canvas->fillRect({10, 10, 200, 100}, {255, 0, 0, 255});   // red rect
       canvas->drawLine({0, 0}, {800, 600}, {255, 255, 255, 255}, 2.0f);
       canvas->strokeRect({50, 50, 300, 200}, {0, 255, 0, 255});

       // End recording and execute
       surface->endFrame();
       surface->flush();

       // Access pixels
       auto pd = surface->getPixelData();
       // pd.data, pd.width, pd.height, pd.rowBytes are ready for display
   }

Frame Cycle
-----------

Every frame follows the same pattern:

1. ``beginFrame(clearColor)`` — prepare the render target (clear to the given
   color; defaults to opaque black).
2. Draw via ``surface->canvas()`` — all commands are *recorded*, not executed
   immediately.
3. ``endFrame()`` — finish recording.
4. ``flush()`` — sort, optimize, and execute the recorded commands on the
   backend.

Text Rendering
--------------

ink supports basic text rendering via ``GlyphCache``:

.. code-block:: cpp

   ink::GlyphCache glyphs;
   glyphs.init("/path/to/font.ttf", 16.0f);
   surface->setGlyphCache(&glyphs);

   surface->beginFrame();
   surface->canvas()->drawText({10, 30}, "Hello, ink!", {255, 255, 255, 255});
   surface->endFrame();
   surface->flush();
