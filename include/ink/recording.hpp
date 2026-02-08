#pragma once

#include "ink/types.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <cstring>

namespace ink {

// Forward declaration
class DrawOpVisitor;

struct DrawOp {
    enum class Type {
        FillRect,
        StrokeRect,
        Line,
        Polyline,
        Text,
        SetClip,
        ClearClip
    };

    Type type;
    Rect rect = {};
    Point p1 = {};
    Point p2 = {};
    std::vector<Point> points;
    std::string text;
    Color color = {};
    f32 width = 1.0f;
};

// Arena allocator for DrawOp string and point data
class DrawOpArena {
public:
    explicit DrawOpArena(size_t initialCapacity = 4096);
    
    // Allocate raw bytes, returns offset
    u32 allocate(size_t bytes);
    
    // Store string, returns offset
    u32 storeString(std::string_view str);
    
    // Store points array, returns offset
    u32 storePoints(const Point* pts, i32 count);
    
    // Access stored data
    const char* getString(u32 offset) const;
    const Point* getPoints(u32 offset) const;
    
    void reset();
    
private:
    std::vector<u8> data_;
};

// Compact DrawOp structure - 28 bytes
struct CompactDrawOp {
    DrawOp::Type type;      // 4 bytes (enum)
    Color color;            // 4 bytes
    f32 width;              // 4 bytes
    
    union Data {
        struct { Rect rect; } fill;                           // 16 bytes
        struct { Rect rect; } stroke;                         // 16 bytes
        struct { Point p1; Point p2; } line;                  // 16 bytes
        struct { u32 offset; u32 count; } polyline;           // 8 bytes
        struct { Point pos; u32 offset; u32 len; } text;      // 16 bytes
        struct { Rect rect; } clip;                           // 16 bytes

        Data() : fill{{}} {}
    } data;                 // 16 bytes
};
// Total: 28 bytes

class Recording {
public:
    Recording(std::vector<CompactDrawOp> ops, DrawOpArena arena);

    const std::vector<CompactDrawOp>& ops() const { return ops_; }
    const DrawOpArena& arena() const { return arena_; }

    // Accept visitor for traversing operations
    void accept(DrawOpVisitor& visitor) const;

private:
    std::vector<CompactDrawOp> ops_;
    DrawOpArena arena_;
};

class Recorder {
public:
    void reset();

    void fillRect(Rect r, Color c);
    void strokeRect(Rect r, Color c, f32 width);
    void drawLine(Point p1, Point p2, Color c, f32 width);
    void drawPolyline(const Point* pts, i32 count, Color c, f32 width);
    void drawText(Point p, std::string_view text, Color c);
    void setClip(Rect r);
    void clearClip();

    std::unique_ptr<Recording> finish();

private:
    std::vector<CompactDrawOp> ops_;
    DrawOpArena arena_;
};

}
