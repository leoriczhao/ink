Multi-Layer Compositing
=======================

ink supports compositing multiple surfaces together using ``drawImage()``.
Each surface can be rendered independently and then combined into a final
output.

Basic Compositing
-----------------

.. code-block:: cpp

   // Create layers
   auto background = ink::Surface::MakeRaster(800, 600);
   auto overlay    = ink::Surface::MakeRaster(800, 600);
   auto composite  = ink::Surface::MakeRaster(800, 600);

   // Render background
   background->beginFrame();
   background->canvas()->fillRect({0, 0, 800, 600}, {30, 30, 30, 255});
   background->endFrame();
   background->flush();

   // Render overlay with transparency
   overlay->beginFrame({0, 0, 0, 0});  // clear to transparent!
   overlay->canvas()->fillRect({100, 100, 200, 200}, {255, 0, 0, 128});
   overlay->endFrame();
   overlay->flush();

   // Composite layers together
   auto bgSnap = background->makeSnapshot();
   auto ovSnap = overlay->makeSnapshot();

   composite->beginFrame();
   composite->canvas()->drawImage(bgSnap, 0, 0);
   composite->canvas()->drawImage(ovSnap, 0, 0);  // alpha-blended on top
   composite->endFrame();
   composite->flush();

Transparent Layers
------------------

When compositing, intermediate layers typically need a **transparent**
background so they don't obscure layers below. Pass ``{0, 0, 0, 0}`` to
``beginFrame()``:

.. code-block:: cpp

   layer->beginFrame({0, 0, 0, 0});  // transparent background

The default clear color is opaque black ``{0, 0, 0, 255}``, which is
appropriate for the final output surface but will hide layers underneath
when used for intermediate compositing layers.

GPU Snapshots
-------------

Snapshots are backend-aware:

- **CPU surfaces** produce CPU-backed images (pixel data in memory).
- **GPU surfaces** produce GPU-backed images (texture handle, no readback).

When a CPU image is drawn on a GPU surface, it is automatically uploaded
and cached via ``GpuContext``. This means you can freely mix CPU and GPU
surfaces in a compositing pipeline.
