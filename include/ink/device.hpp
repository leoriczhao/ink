#pragma once

/**
 * @file device.hpp
 * @brief Recording device that converts Canvas commands into low-level draw operations.
 */

#include "ink/types.hpp"
#include "ink/recording.hpp"
#include <memory>

namespace ink {

class GlyphCache;
class Image;

/**
 * @brief The single recording device.
 *
 * Device converts Canvas high-level commands into low-level Recording ops.
 * All Surfaces use the same Device — it always records, never draws directly.
 * The actual rendering is done by a Backend that consumes the Recording.
 */
class Device {
public:
    Device() = default;
    ~Device() = default;

    /// @brief Begin recording a new frame (resets the recorder).
    void beginFrame();
    /// @brief End the current frame recording.
    void endFrame();

    /// @name Drawing commands
    /// @{

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

    /// @}

    /// @brief Set the current clip rectangle.
    /// @param r The clipping rectangle.
    void setClipRect(Rect r);

    /// @brief Clear the current clip rectangle.
    void resetClip();

    /// @brief Finish recording and return the immutable Recording.
    /// @return Unique pointer to the completed Recording.
    std::unique_ptr<Recording> finishRecording();

private:
    Recorder recorder_;
    std::unique_ptr<Recording> recording_;
};

}
