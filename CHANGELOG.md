# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Changed
- Release version metadata is generated at CMake configure time from `-DINK_VERSION`, a source-package `VERSION` file, or the nearest Git tag instead of hard-coded source edits.
- Stable release packaging now installs generated headers through `cmake --install` before creating binary tarballs.

### Removed
- Removed `scripts/bump-version.sh`; stable releases now require a changelog PR plus `scripts/release-preflight.sh X.Y.Z` before tagging.

## [0.3.0] - 2026-04-25

### Added
- **Path API**: `Path` with move, line, quad, cubic, conic, close, relative commands, transforms, bounds, contains, direction, and convexity checks.
- **Path builders**: `addRect`, `addOval`, `addCircle`, `addRoundRect`, `addArc`, `addPoly`, and `addPath`.
- **Path rendering**: `Canvas::drawPath`, `Device::drawPath`, `Recorder::fillPath`, and `Recorder::strokePath`.
- **CPU path rasterizer**: Flattened curve rendering with winding/even-odd fill rules and optional supersampled antialiasing.
- **Stroke engine**: Stroke-to-fill conversion with butt/round/square caps, miter/round/bevel joins, and miter limit.
- **Path effects**: `PathEffect` abstraction and dash effect factory via `PathEffects::MakeDash`.
- **Path clipping**: `Canvas::clipPath` and `Recorder::setClipPath` with CPU clip-mask support.
- **GL path rendering**: Solid filled and stroked paths use stencil-and-cover rendering on OpenGL; `clipPath` writes directly into the GL stencil buffer. Shader-backed path fills keep the CPU texture fallback until GL shader generation lands.

### Changed
- Paint now carries optional shader, color filter, path effect, stroke cap/join, antialiasing, and image filter quality fields.
- DrawOpVisitor and Recording now support path draw ops, path clip ops, image-rect draw ops, shaders, and color filters.
- GL vertex paths now apply the current Canvas matrix before batching.

## [0.2.0] - 2026-04-03

### Added
- **Transform stack**: translate(), rotate(), scale(), concat(), setMatrix() with save/restore
- **Matrix type**: 3x3 affine transform with mapPoint, mapRect, inverted, concatenation
- **Paint abstraction**: Paint struct with color, strokeWidth, opacity, blendMode, style
- **Blend modes**: All 11 Porter-Duff compositing modes (SrcOver, Src, Dst, SrcIn, DstIn, SrcOut, DstOut, SrcAtop, DstAtop, Xor, Clear)
- **Opacity**: Per-draw opacity multiplier via Paint
- **New primitives**: fillCircle, strokeCircle, fillRoundRect, strokeRoundRect
- **Canvas::draw()**: Paint-based drawing API for advanced compositing
- **GPU blend modes**: GL backend maps all Porter-Duff modes to glBlendFunc

### Changed
- DrawOpVisitor methods now receive BlendMode and opacity parameters
- CompactDrawOp stores blendMode and opacity in former padding bytes (zero size increase)
- CpuRenderer uses format-aware pixel encoding/decoding (BGRA and RGBA)
- Canvas Color-based methods preserved as convenience wrappers

### Fixed
- Arena alignment UB (misaligned Point* cast after odd-length string)
- Pixmap::clear() stride bug (row-by-row iteration for padded buffers)
- CpuRenderer pixel format hardcoding (now format-aware)
- GlyphCache::drawText() alpha override (preserves destination alpha)
- visitText stride mismatch (width vs stride/bpp)

## [0.1.1] - 2026-02-24

### Fixed
- Aligned version references across all files (Doxyfile, version.hpp, docs/conf.py were incorrectly set to 0.2.0 in v0.1.0)
- Enhanced `release-preflight.sh` to check Doxyfile, version.hpp, and docs/conf.py
- Added `scripts/bump-version.sh` for unified version bumping

### Note
- This is a hotfix for v0.1.0. Use v0.1.1 instead of v0.1.0.

## [0.1.0] - 2025-01-30 *(deprecated — use v0.1.1)*

### Added
- Initial release
- Canvas API with save/restore state stack and clip management
- Renderer abstraction with CPU and GPU implementations
- CPU software rasterizer
- OpenGL 3.3+ renderer via FBO + GLSL shaders, vertex batching, scissor clipping
- Metal renderer (macOS/iOS)
- GPU snapshots (no readback required for GPU-to-GPU compositing)
- Multi-layer surface compositing
- Text rendering via GlyphCache
- Recording-only surface for command capture
