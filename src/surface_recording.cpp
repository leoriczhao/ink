#include "ink/surface.hpp"
#include "ink/device.hpp"

namespace ink {

std::unique_ptr<Surface> Surface::MakeRecording(i32 w, i32 h) {
    auto device = std::make_unique<GpuDevice>();
    device->resize(w, h);
    return std::unique_ptr<Surface>(new Surface(std::move(device), nullptr, nullptr));
}

}
