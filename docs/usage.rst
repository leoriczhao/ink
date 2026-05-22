Usage
=====

Basic Drawing
-------------

.. code-block:: cpp

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

Text Rendering
--------------

.. code-block:: cpp

   ink::GlyphCache glyphs;
   glyphs.init("/path/to/font.ttf", 16.0f);
   surface->setGlyphCache(&glyphs);

   surface->beginFrame();
   surface->canvas()->drawText({10, 30}, "Hello, ink!", {255, 255, 255, 255});
   surface->endFrame();
   surface->flush();

GPU Rendering
-------------

.. code-block:: cpp

   #include <ink/ink.hpp>
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

Paths, Clipping, and Dash Effects
---------------------------------

.. code-block:: cpp

   ink::Path path;
   path.moveTo(100, 100)
       .cubicTo(160, 20, 260, 180, 320, 100)
       .lineTo(320, 240)
       .lineTo(100, 240)
       .close();

   ink::Paint fill = ink::Paint::Fill({40, 120, 220, 255});
   fill.antiAlias = true;

   surface->beginFrame();
   auto* canvas = surface->canvas();
   canvas->save();
   canvas->clipPath(path);
   canvas->fillRect({0, 0, 800, 600}, {245, 245, 245, 255});
   canvas->restore();
   canvas->drawPath(path, fill);

   ink::Paint stroke = ink::Paint::Stroke({255, 255, 255, 255}, 4.0f);
   float dash[] = {12.0f, 6.0f};
   stroke.pathEffect = ink::PathEffects::MakeDash(dash, 2);
   canvas->drawPath(path, stroke);
   surface->endFrame();
   surface->flush();

Multi-layer Compositing
-----------------------

.. code-block:: cpp

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
