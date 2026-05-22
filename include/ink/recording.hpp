#pragma once

/**
 * @file recording.hpp
 * @brief Draw operation types, arena allocator, recording, and recorder.
 */

#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include "ink/path.hpp"
#include "ink/types.hpp"
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

namespace ink {

class DrawOpVisitor;
class Image;
class Shader;
class ColorFilter;

/// @brief Draw operation type tag.
struct DrawOp {
  enum class Type : u8 {
    FillRect,
    StrokeRect,
    Line,
    Polyline,
    Text,
    DrawImage,
    SetClip,
    SetClipPath,
    ClearClip,
    SetTransform,
    ClearTransform,
    FillCircle,
    StrokeCircle,
    FillRoundRect,
    StrokeRoundRect,
    FillPath,
    StrokePath,
    DrawImageRect,
  };
};

/// @brief Arena allocator for variable-length DrawOp data.
class DrawOpArena {
public:
  explicit DrawOpArena(size_t initialCapacity = 4096);

  u32 allocate(size_t bytes);
  u32 storeString(std::string_view str);
  u32 storePoints(const Point *pts, i32 count);
  const char *getString(u32 offset) const;
  const Point *getPoints(u32 offset) const;
  u32 storeMatrix(const Matrix &m);
  const Matrix *getMatrix(u32 offset) const;
  u32 storeRoundRect(Rect r, f32 rx, f32 ry);
  void getRoundRect(u32 offset, Rect &r, f32 &rx, f32 &ry) const;

  /// @brief Store two rects (src + dst) for drawImageRect.
  u32 storeRectPair(Rect src, Rect dst);
  void getRectPair(u32 offset, Rect &src, Rect &dst) const;

  void reset();

private:
  std::vector<u8> data_;
};

/// @brief Compact draw operation structure (32 bytes).
struct CompactDrawOp {
  DrawOp::Type type;
  BlendMode blendMode = BlendMode::SrcOver;
  u8 opacity = 255;
  /// Packed flags: [1:0] cap, [3:2] join, [4] fillRule(0=winding,1=evenodd),
  /// [5] antiAlias, [7:6] filterQuality
  u8 flags = 0x20;
  Color color;
  f32 width;
  u16 shaderIndex = 0xFFFF;      ///< 0xFFFF = no shader.
  u16 colorFilterIndex = 0xFFFF; ///< 0xFFFF = no color filter.

  union Data {
    struct {
      Rect rect;
    } fill;
    struct {
      Rect rect;
    } stroke;
    struct {
      Point p1;
      Point p2;
    } line;
    struct {
      u32 offset;
      u32 count;
    } polyline;
    struct {
      Point pos;
      u32 offset;
      u32 len;
    } text;
    struct {
      f32 x;
      f32 y;
      u32 imageIndex;
    } image;
    struct {
      Rect rect;
    } clip;
    struct {
      u32 matrixOffset;
    } transform;
    struct {
      f32 cx, cy, radius;
    } circle;
    struct {
      u32 arenaOffset;
    } roundRect;
    struct {
      u32 pathIndex;
      f32 miterLimit;
    } path;
    struct {
      u32 imageIndex;
      u32 arenaOffset;
    } imageRect;

    Data() : fill{{}} {}
  } data;
};

/// @brief Immutable command buffer containing recorded draw operations.
class Recording {
public:
  Recording(std::vector<CompactDrawOp> ops, DrawOpArena arena,
            std::vector<std::shared_ptr<Image>> images, std::vector<Path> paths,
            std::vector<std::shared_ptr<Shader>> shaders,
            std::vector<std::shared_ptr<ColorFilter>> colorFilters);

  const std::vector<CompactDrawOp> &ops() const { return ops_; }
  const DrawOpArena &arena() const { return arena_; }
  const std::vector<std::shared_ptr<Image>> &images() const { return images_; }
  const std::vector<Path> &paths() const { return paths_; }
  const std::vector<std::shared_ptr<Shader>> &shaders() const {
    return shaders_;
  }
  const std::vector<std::shared_ptr<ColorFilter>> &colorFilters() const {
    return colorFilters_;
  }

  const Image *getImage(u32 index) const;
  const Path *getPath(u32 index) const;
  const Shader *getShader(u16 index) const;
  const ColorFilter *getColorFilter(u16 index) const;

  void accept(DrawOpVisitor &visitor) const;
  void dispatch(DrawOpVisitor &visitor, const class DrawPass &pass) const;

private:
  void dispatchOp(const CompactDrawOp &op, DrawOpVisitor &visitor) const;
  std::vector<CompactDrawOp> ops_;
  DrawOpArena arena_;
  std::vector<std::shared_ptr<Image>> images_;
  std::vector<Path> paths_;
  std::vector<std::shared_ptr<Shader>> shaders_;
  std::vector<std::shared_ptr<ColorFilter>> colorFilters_;
};

/// @brief Records draw operations into a compact command buffer.
class Recorder {
public:
  void reset();

  // --- Basic draw ops (Color-based) ---
  void fillRect(Rect r, Color c);
  void strokeRect(Rect r, Color c, f32 width);
  void drawLine(Point p1, Point p2, Color c, f32 width);
  void drawPolyline(const Point *pts, i32 count, Color c, f32 width);
  void drawText(Point p, std::string_view text, Color c);
  void drawImage(std::shared_ptr<Image> image, f32 x, f32 y);
  void setClip(Rect r);
  void setClipPath(const Path &path, bool antiAlias = true);
  void clearClip();
  void setTransform(const Matrix &m);
  void clearTransform();
  void fillCircle(f32 cx, f32 cy, f32 radius, Color c);
  void strokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width);
  void fillRoundRect(Rect r, f32 rx, f32 ry, Color c);
  void strokeRoundRect(Rect r, f32 rx, f32 ry, Color c, f32 width);

  // --- Paint-based overloads ---
  void fillRect(Rect r, const Paint &p);
  void strokeRect(Rect r, const Paint &p);
  void drawLine(Point p1, Point p2, const Paint &p);
  void drawPolyline(const Point *pts, i32 count, const Paint &p);
  void drawText(Point pos, std::string_view text, const Paint &p);
  void fillCircle(f32 cx, f32 cy, f32 radius, const Paint &p);
  void strokeCircle(f32 cx, f32 cy, f32 radius, const Paint &p);
  void fillRoundRect(Rect r, f32 rx, f32 ry, const Paint &p);
  void strokeRoundRect(Rect r, f32 rx, f32 ry, const Paint &p);

  // --- Path ops ---
  void fillPath(const Path &path, const Paint &p);
  void strokePath(const Path &path, const Paint &p);

  // --- Image rect ---
  void drawImageRect(std::shared_ptr<Image> image, Rect src, Rect dst,
                     const Paint &p);

  std::unique_ptr<Recording> finish();

private:
  void applyPaintToOp(CompactDrawOp &op, const Paint &p);
  u16 storeShader(const std::shared_ptr<Shader> &s);
  u16 storeColorFilter(const std::shared_ptr<ColorFilter> &f);

  std::vector<CompactDrawOp> ops_;
  DrawOpArena arena_;
  std::vector<std::shared_ptr<Image>> images_;
  std::vector<Path> paths_;
  std::vector<std::shared_ptr<Shader>> shaders_;
  std::vector<std::shared_ptr<ColorFilter>> colorFilters_;
};

} // namespace ink
