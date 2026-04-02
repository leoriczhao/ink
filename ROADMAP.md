# ink Roadmap

Current version: **0.1.1**

## v0.2.0 — Foundation

Transform stack, Paint abstraction, blend modes, and new primitives.

- **Transform stack**: 2D affine transforms (translate, rotate, scale) with save/restore
- **Paint**: Replace bare Color with Paint struct (color, stroke width, opacity, blend mode, style)
- **Blend modes**: Porter-Duff compositing (SrcOver, Src, Dst, SrcIn, DstIn, SrcOut, DstOut, SrcAtop, DstAtop, Xor, Clear)
- **Opacity**: Per-draw opacity multiplier
- **New primitives**: Rounded rect, circle, ellipse, arc

## v0.3.0 — Paths and Curves

General-purpose vector path type — the heart of a 2D engine.

- **Path class**: moveTo, lineTo, quadTo, cubicTo, close, convenience builders (addRect, addOval, addRoundRect)
- **Path fill**: Even-odd and winding fill rules via scanline rasterizer
- **Path stroke**: Stroke-to-fill conversion with line joins (miter/round/bevel) and caps (butt/round/square)
- **Dash patterns**: Dashed/dotted line styles
- Rewrite existing primitives as Path sugar

## v0.4.0 — Anti-Aliasing and Gradients

Visual quality and rich fill styles.

- **Anti-aliasing**: Analytic AA for path edges (coverage-based)
- **Gradient shaders**: Linear, radial, sweep (conic)
- **Pattern shader**: Tiled image fills
- **Paint shader abstraction**: Gradient/pattern plugs into Paint
- GPU shader infrastructure for gradients

## v0.5.0 — Text and Image Enhancements

Production-quality text and image pipeline.

- **Font shaping**: HarfBuzz integration, Unicode/BiDi, font fallback
- **Sub-pixel text rendering**
- **Image scaling**: Quality filters (bilinear, bicubic, Lanczos)
- **drawImageRect**: Source/dest rect with sub-region support
- Color space management (sRGB, linear)

## v1.0.0 — Production Ready

Performance, polish, completeness.

- **Filters**: Mask filters (blur), color filters (matrix, HSL), image filters (drop shadow)
- **Clip paths**: Non-rectangular clipping
- **Save layer**: Isolated compositing groups with alpha
- **Performance**: SIMD pixel ops, tiled rendering, parallel rasterization
- **Vulkan backend**: Complete implementation
- API stability guarantee, comprehensive benchmarks
