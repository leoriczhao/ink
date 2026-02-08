#pragma once

#include "ink/types.hpp"
#include <string_view>
#include <vector>
#include <memory>

namespace ink {

class Device;
class Image;

class Canvas {
public:
    explicit Canvas(Device* device);

    void fillRect(Rect r, Color c);
    void strokeRect(Rect r, Color c, f32 width = 1.0f);
    void drawLine(Point p1, Point p2, Color c, f32 width = 1.0f);
    void drawPolyline(const Point* pts, i32 count, Color c, f32 width = 1.0f);
    void drawText(Point p, std::string_view text, Color c);
    void drawImage(std::shared_ptr<Image> image, f32 x, f32 y);

    void save();
    void restore();
    void clipRect(Rect r);

private:
    struct State {
        bool hasClip = false;
        Rect clip = {};
    };

    Device* device_ = nullptr;
    std::vector<State> stack_;
    State current_;

    void applyClip();
};

}
