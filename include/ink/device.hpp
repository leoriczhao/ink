#pragma once

/**
 * @file device.hpp
 * @brief Recording device that converts Canvas commands into low-level draw operations.
 */

#include "ink/types.hpp"
#include "ink/matrix.hpp"
#include "ink/paint.hpp"
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
 * The actual rendering is done by a Renderer that consumes the Recording.
 */
class Device {
public:
    Device() = default;
    ~Device() = default;

    /// @brief Begin recording a new frame (resets the recorder).
    void beginFrame();
    /// @brief End the current frame recording.
    void endFrame();

    /// @name Drawing commands (Paint-based)
    /// @{

    void fillRect(Rect r, const Paint& paint);
    void strokeRect(Rect r, const Paint& paint);
    void drawLine(Point p1, Point p2, const Paint& paint);
    void drawPolyline(const Point* pts, i32 count, const Paint& paint);
    void drawText(Point p, std::string_view text, const Paint& paint);
    void drawImage(std::shared_ptr<Image> image, f32 x, f32 y);

    /// @}

    /// @brief Set the current clip rectangle.
    void setClipRect(Rect r);
    /// @brief Clear the current clip rectangle.
    void resetClip();

    /// @brief Set the current transformation matrix.
    void setTransform(const Matrix& m);
    /// @brief Clear the current transformation matrix.
    void clearTransform();

    /// @brief Finish recording and return the immutable Recording.
    std::unique_ptr<Recording> finishRecording();

private:
    Recorder recorder_;
    std::unique_ptr<Recording> recording_;
};

}
