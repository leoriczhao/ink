#pragma once

/**
 * Ink - A lightweight 2D rendering library
 *
 * Usage:
 *
 *   // CPU rendering
 *   #include <ink/ink.hpp>
 *   auto surface = ink::Surface::MakeRaster(800, 600);
 *   surface->canvas()->fillRect({0, 0, 100, 100}, {255, 0, 0, 255});
 *   surface->flush();
 *
 *   // GPU rendering (requires #include <ink/gpu/gl/gl_context.hpp>)
 *   auto ctx = ink::GpuContexts::MakeGL();
 *   auto surface = ink::Surface::MakeGpu(ctx, 800, 600);
 *   surface->canvas()->fillRect({0, 0, 100, 100}, {255, 0, 0, 255});
 *   surface->flush();
 */

// Version
#include "ink/version.hpp"

// Core types
#include "ink/types.hpp"

// Pixel data
#include "ink/pixmap.hpp"
#include "ink/pixel_data.hpp"

// Image (immutable pixel snapshot)
#include "ink/image.hpp"

// Recording and commands
#include "ink/recording.hpp"
#include "ink/draw_op_visitor.hpp"
#include "ink/draw_pass.hpp"

// Device (recording device)
#include "ink/device.hpp"

// Canvas (user-facing drawing API)
#include "ink/canvas.hpp"

// GPU context (abstract)
#include "ink/gpu/gpu_context.hpp"

// GL context factory (conditional - include <ink/gpu/gl/gl_context.hpp> explicitly)
#if INK_HAS_GL
#include "ink/gpu/gl/gl_context.hpp"
#endif

// Pipeline (GPU pipeline stages)
#include "ink/pipeline.hpp"

// Surface (top-level rendering target)
#include "ink/surface.hpp"

// Text rendering
#include "ink/glyph_cache.hpp"
