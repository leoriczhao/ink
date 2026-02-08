#pragma once

#include <cstdint>

namespace ink {

using i32 = int32_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u16 = uint16_t;
using u8 = uint8_t;
using f32 = float;
using f64 = double;

struct Point { f32 x = 0, y = 0; };
struct Rect { f32 x = 0, y = 0, w = 0, h = 0; };
struct Color { u8 r = 0, g = 0, b = 0, a = 255; };

}
