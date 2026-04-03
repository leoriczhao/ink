# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
