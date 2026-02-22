#pragma once

/**
 * @file recording.hpp
 * @brief Draw operation types, arena allocator, recording, and recorder.
 */

#include "ink/types.hpp"
#include <string_view>
#include <vector>
#include <memory>
#include <cstring>

namespace ink {

// Forward declarations
class DrawOpVisitor;
class Image;

/// @brief Draw operation type tag.
struct DrawOp {
    /// @brief Operation type enumeration.
    enum class Type : u8 {
        FillRect,    ///< Fill a rectangle.
        StrokeRect,  ///< Stroke a rectangle outline.
        Line,        ///< Draw a line segment.
        Polyline,    ///< Draw connected line segments.
        Text,        ///< Draw text.
        DrawImage,   ///< Draw an image.
        SetClip,     ///< Set a clip rectangle.
        ClearClip    ///< Clear the clip rectangle.
    };
};

/// @brief Arena allocator for variable-length DrawOp data (strings, point arrays).
class DrawOpArena {
public:
    /// @brief Construct an arena with the given initial capacity.
    /// @param initialCapacity Initial byte capacity (default 4096).
    explicit DrawOpArena(size_t initialCapacity = 4096);

    /// @brief Allocate raw storage.
    /// @param bytes Number of bytes to allocate.
    /// @return Byte offset into the arena.
    u32 allocate(size_t bytes);

    /// @brief Store a string in the arena.
    /// @param str The string to store.
    /// @return Byte offset to the stored string.
    u32 storeString(std::string_view str);

    /// @brief Store a points array in the arena.
    /// @param pts Pointer to the points array.
    /// @param count Number of points.
    /// @return Byte offset to the stored points.
    u32 storePoints(const Point* pts, i32 count);

    /// @brief Retrieve a stored string by offset.
    /// @param offset Byte offset returned by storeString().
    /// @return Pointer to the null-terminated string.
    const char* getString(u32 offset) const;

    /// @brief Retrieve stored points by offset.
    /// @param offset Byte offset returned by storePoints().
    /// @return Pointer to the first Point.
    const Point* getPoints(u32 offset) const;

    /// @brief Reset the arena, discarding all stored data.
    void reset();

private:
    std::vector<u8> data_;
};

/// @brief Compact draw operation structure (28 bytes).
struct CompactDrawOp {
    DrawOp::Type type;      ///< Operation type (1 byte).
    u8 padding[3];          ///< Alignment padding (3 bytes).
    Color color;            ///< Draw color (4 bytes).
    f32 width;              ///< Line/stroke width (4 bytes).

    /// @brief Union of per-operation data variants.
    union Data {
        struct { Rect rect; } fill;                           ///< FillRect data.
        struct { Rect rect; } stroke;                         ///< StrokeRect data.
        struct { Point p1; Point p2; } line;                  ///< Line data.
        struct { u32 offset; u32 count; } polyline;           ///< Polyline data (arena offset + count).
        struct { Point pos; u32 offset; u32 len; } text;      ///< Text data (position + arena offset + length).
        struct { f32 x; f32 y; u32 imageIndex; } image;       ///< DrawImage data.
        struct { Rect rect; } clip;                           ///< SetClip data.

        Data() : fill{{}} {}
    } data;                 ///< Per-operation payload (16 bytes).
};

/// @brief Immutable command buffer containing recorded draw operations.
///
/// Created by Recorder::finish(). Operations can be traversed in original
/// order via accept() or in sorted order via dispatch().
class Recording {
public:
    /// @brief Construct a Recording from operations, arena, and images.
    /// @param ops Vector of compact draw operations.
    /// @param arena Arena holding variable-length data.
    /// @param images Vector of referenced images.
    Recording(std::vector<CompactDrawOp> ops, DrawOpArena arena,
              std::vector<std::shared_ptr<Image>> images);

    /// @brief Get the list of recorded operations.
    const std::vector<CompactDrawOp>& ops() const { return ops_; }
    /// @brief Get the data arena.
    const DrawOpArena& arena() const { return arena_; }
    /// @brief Get the list of referenced images.
    const std::vector<std::shared_ptr<Image>>& images() const { return images_; }

    /// @brief Get an image by index.
    /// @param index Index into the images list.
    /// @return Pointer to the Image, or nullptr if out of range.
    const Image* getImage(u32 index) const;

    /// @brief Traverse operations in original recording order.
    /// @param visitor The visitor to receive each operation.
    void accept(DrawOpVisitor& visitor) const;

    /// @brief Dispatch operations in sorted order defined by a draw pass.
    /// @param visitor The visitor to receive each operation.
    /// @param pass The draw pass defining execution order.
    void dispatch(DrawOpVisitor& visitor, const class DrawPass& pass) const;

private:
    void dispatchOp(const CompactDrawOp& op, DrawOpVisitor& visitor) const;
    std::vector<CompactDrawOp> ops_;
    DrawOpArena arena_;
    std::vector<std::shared_ptr<Image>> images_;
};

/// @brief Records draw operations into a compact command buffer.
///
/// Call drawing methods to accumulate operations, then finish() to produce
/// an immutable Recording.
class Recorder {
public:
    /// @brief Reset the recorder, discarding all accumulated operations.
    void reset();

    /// @brief Record a fill-rectangle operation.
    /// @param r Rectangle to fill.
    /// @param c Fill color.
    void fillRect(Rect r, Color c);

    /// @brief Record a stroke-rectangle operation.
    /// @param r Rectangle to stroke.
    /// @param c Stroke color.
    /// @param width Stroke line width.
    void strokeRect(Rect r, Color c, f32 width);

    /// @brief Record a line-drawing operation.
    /// @param p1 Start point.
    /// @param p2 End point.
    /// @param c Line color.
    /// @param width Line width.
    void drawLine(Point p1, Point p2, Color c, f32 width);

    /// @brief Record a polyline-drawing operation.
    /// @param pts Array of vertices.
    /// @param count Number of points.
    /// @param c Line color.
    /// @param width Line width.
    void drawPolyline(const Point* pts, i32 count, Color c, f32 width);

    /// @brief Record a text-drawing operation.
    /// @param p Position of the text baseline.
    /// @param text UTF-8 text to draw.
    /// @param c Text color.
    void drawText(Point p, std::string_view text, Color c);

    /// @brief Record an image-drawing operation.
    /// @param image Shared pointer to the image.
    /// @param x X position.
    /// @param y Y position.
    void drawImage(std::shared_ptr<Image> image, f32 x, f32 y);

    /// @brief Record a set-clip operation.
    /// @param r The clipping rectangle.
    void setClip(Rect r);

    /// @brief Record a clear-clip operation.
    void clearClip();

    /// @brief Finish recording and produce an immutable Recording.
    /// @return Unique pointer to the completed Recording.
    std::unique_ptr<Recording> finish();

private:
    std::vector<CompactDrawOp> ops_;
    DrawOpArena arena_;
    std::vector<std::shared_ptr<Image>> images_;
};

}
