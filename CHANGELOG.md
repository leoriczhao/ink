# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

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
