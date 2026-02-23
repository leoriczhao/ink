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

## [0.2.0] - 2025-02-16

### Changed
- **Breaking**: Removed `GpuContextAdapter` and `GpuContext::Impl` indirection layers
- `GpuContext` now directly holds `shared_ptr<GpuImpl>`
- `Surface` holds `shared_ptr<Renderer>` allowing `GpuContext` to be used directly
- Extracted duplicated dispatch logic into `Recording::dispatchOp`
- `GLImpl` now inherits `DrawOpVisitor`
- Added `setGlyphCache` to `Renderer` base class
- Added FBO completeness check in GL backend

### Added
- Separated GL-specific interop from `GpuImpl` (Phase 4)
- Implemented `Renderer` abstraction and split GL resources (Phase 1-3)

### Removed
- Backend layer - `GpuContext` now handles rendering directly

## [0.1.0] - 2025-01-30 *(deprecated — use v0.1.1)*

### Added
- Initial release
- Canvas API with save/restore state stack and clip management
- CPU software rasterizer backend
- OpenGL 3.3+ backend with FBO + GLSL shaders, vertex batching, scissor clipping
- GPU snapshots (no readback required for GPU-to-GPU compositing)
- Multi-layer surface compositing
- Text rendering via GlyphCache
- Recording-only surface for command capture
