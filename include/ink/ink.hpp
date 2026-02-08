#pragma once

// Ink - A lightweight 2D rendering library

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

// Backend (rendering execution)
#include "ink/backend.hpp"
#include "ink/cpu_backend.hpp"

// Pipeline (GPU pipeline stages)
#include "ink/pipeline.hpp"

// Surface (top-level rendering target)
#include "ink/surface.hpp"

// Text rendering
#include "ink/glyph_cache.hpp"
