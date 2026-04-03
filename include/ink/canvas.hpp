#pragma once

/**
 * @file canvas.hpp
 * @brief User-facing drawing API with state management.
 */

#include "ink/types.hpp"
#include "ink/matrix.hpp"
#include "ink/paint.hpp"
#include <string_view>
#include <vector>
#include <memory>

namespace ink {

class Device;
class Image;

/// @brief User-facing drawing API.
///
/// Canvas provides high-level drawing commands that are recorded through a
/// Device. Use save()/restore() for state management and clipRect() for
/// clipping.
class Canvas {
public:
    /// @brief Construct a Canvas that records into a device.
    /// @param device The device to record drawing commands into.
    explicit Canvas(Device* device);

    /// @brief Fill a rectangle with a solid color.
    /// @param r The rectangle to fill.
    /// @param c Fill color.
    void fillRect(Rect r, Color c);

    /// @brief Stroke a rectangle outline.
    /// @param r The rectangle to stroke.
    /// @param c Stroke color.
    /// @param width Stroke line width (default 1.0).
    void strokeRect(Rect r, Color c, f32 width = 1.0f);

    /// @brief Draw a line between two points.
    /// @param p1 Start point.
    /// @param p2 End point.
    /// @param c Line color.
    /// @param width Line width (default 1.0).
    void drawLine(Point p1, Point p2, Color c, f32 width = 1.0f);

    /// @brief Draw a connected series of line segments.
    /// @param pts Array of vertices.
    /// @param count Number of points.
    /// @param c Line color.
    /// @param width Line width (default 1.0).
    void drawPolyline(const Point* pts, i32 count, Color c, f32 width = 1.0f);

    /// @brief Draw text at the given position.
    /// @param p Position of the text baseline.
    /// @param text UTF-8 text to draw.
    /// @param c Text color.
    void drawText(Point p, std::string_view text, Color c);

    /// @brief Draw an image at the given position (alpha-blended).
    /// @param image Shared pointer to the image.
    /// @param x X position.
    /// @param y Y position.
    void drawImage(std::shared_ptr<Image> image, f32 x, f32 y);

    /// @brief Fill a circle with a solid color.
    /// @param cx Center X.
    /// @param cy Center Y.
    /// @param radius Circle radius.
    /// @param c Fill color.
    void fillCircle(f32 cx, f32 cy, f32 radius, Color c);

    /// @brief Stroke a circle outline.
    /// @param cx Center X.
    /// @param cy Center Y.
    /// @param radius Circle radius.
    /// @param c Stroke color.
    /// @param width Stroke line width (default 1.0).
    void strokeCircle(f32 cx, f32 cy, f32 radius, Color c, f32 width = 1.0f);

    /// @brief Fill a rounded rectangle with a solid color.
    /// @param r The rectangle.
    /// @param rx Corner radius in X.
    /// @param ry Corner radius in Y.
    /// @param c Fill color.
    void fillRoundRect(Rect r, f32 rx, f32 ry, Color c);

    /// @brief Stroke a rounded rectangle outline.
    /// @param r The rectangle.
    /// @param rx Corner radius in X.
    /// @param ry Corner radius in Y.
    /// @param c Stroke color.
    /// @param width Stroke line width (default 1.0).
    void strokeRoundRect(Rect r, f32 rx, f32 ry, Color c, f32 width = 1.0f);

    /// @name Paint-based drawing (advanced)
    /// @{

    /// @brief Draw a rectangle with a Paint (supports blend modes, opacity).
    void draw(Rect r, const Paint& paint);

    /// @brief Draw a line with a Paint (supports blend modes, opacity).
    void draw(Point p1, Point p2, const Paint& paint);

    /// @}

    /// @brief Save the current clip state onto the stack.
    void save();

    /// @brief Restore the most recently saved clip state.
    void restore();

    /// @brief Intersect the current clip with a rectangle.
    /// @param r The clipping rectangle.
    void clipRect(Rect r);

    /// @brief Translate the current transform.
    void translate(f32 dx, f32 dy);

    /// @brief Rotate the current transform.
    void rotate(f32 radians);

    /// @brief Scale the current transform.
    void scale(f32 sx, f32 sy);

    /// @brief Post-concatenate a matrix onto the current transform.
    void concat(const Matrix& m);

    /// @brief Replace the current transform with the given matrix.
    void setMatrix(const Matrix& m);

    /// @brief Return the current transform matrix.
    Matrix getMatrix() const;

private:
    struct State {
        bool hasClip = false;
        Rect clip = {};
        Matrix transform;
    };

    Device* device_ = nullptr;
    std::vector<State> stack_;
    State current_;

    void applyClip();
    void applyTransform();
};

}
