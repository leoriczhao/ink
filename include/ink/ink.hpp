#pragma once

/**
 * @file ink.hpp
 * @brief Convenience header that includes the entire ink public API.
 */

// Version
#include "ink/version.hpp"

// Core types
#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include "ink/types.hpp"

// Path
#include "ink/path.hpp"
#include "ink/path_effect.hpp"

// Pixel data
#include "ink/pixel_data.hpp"
#include "ink/pixmap.hpp"

// Image
#include "ink/image.hpp"

// Shaders and filters
#include "ink/color_filter.hpp"
#include "ink/shader.hpp"

// Recording and commands
#include "ink/draw_op_visitor.hpp"
#include "ink/draw_pass.hpp"
#include "ink/recording.hpp"

// Device
#include "ink/device.hpp"

// Canvas
#include "ink/canvas.hpp"

// GPU context
#include "ink/gpu/gpu_context.hpp"

#if INK_HAS_GL
#include "ink/gpu/gl/gl_context.hpp"
#endif

// Surface
#include "ink/surface.hpp"

// Text
#include "ink/font.hpp"
#include "ink/glyph_cache.hpp"
#include "ink/text_blob.hpp"
#include "ink/typeface.hpp"
